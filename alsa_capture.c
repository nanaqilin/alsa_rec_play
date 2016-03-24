/*
This example reads from the default PCM device
and writes to standard output for 5 seconds of data.
*/
/* Use the newer ALSA API */
#include <stdio.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#define LENGTH    10   //录音时间,秒
#define RATE    8000 //采样频率
#define SIZE    16   //量化位数
#define CHANNELS 1   //声道数目
#define RSIZE    64    //buf的大小



/********以下是wave格式文件的文件头格式说明******/
/*------------------------------------------------
|             RIFF WAVE Chunk                  |
|             ID = 'RIFF'                     |
|             RiffType = 'WAVE'                |
------------------------------------------------
|             Format Chunk                     |
|             ID = 'fmt '                      |
------------------------------------------------
|             Fact Chunk(optional)             |
|             ID = 'fact'                      |
------------------------------------------------
|             Data Chunk                       |
|             ID = 'data'                      |
------------------------------------------------*/
/**********以上是wave文件格式头格式说明***********/
/*wave 文件一共有四个Chunk组成，其中第三个Chunk可以省略，每个Chunk有标示（ID）,
大小（size,就是本Chunk的内容部分长度）,内容三部分组成*/
#pragma pack(1) 
typedef struct WAVEHEAD
{
    /****RIFF WAVE CHUNK*/
    unsigned char riff_head[4];//四个字节存放'R','I','F','F'
    int riffdata_len;        //整个文件的长度-8;每个Chunk的size字段，都是表示除了本Chunk的ID和SIZE字段外的长度;
    unsigned char wave_head[4];//四个字节存放'W','A','V','E'
    /****RIFF WAVE CHUNK*/
    /****Format CHUNK*/
    unsigned char fmt_head[4];//四个字节存放'f','m','t',''
    int fmtdata_len;       //16后没有附加消息，18后有附加消息；一般为16，其他格式转来的话为18
    short int format_tag;       //编码方式，一般为0x0001;
    short int channels;       //声道数目，1单声道，2双声道;
    int samples_persec;        //采样频率;
    int bytes_persec;        //每秒所需字节数;
    short int block_align;       //每个采样需要多少字节，若声道是双，则两个一起考虑;
    short int bits_persec;       //即量化位数
    /****Format CHUNK*/
    /***Data Chunk**/
    unsigned char data_head[4];//四个字节存放'd','a','t','a'
    int data_len;        //语音数据部分长度，不包括文件头的任何部分
}WAVE_HEAD; //定义WAVE文件的文件头结构体
#pragma pack()


int main()
{

    WAVE_HEAD wavehead;
    /*以下wave 文件头赋值*/
    memcpy(wavehead.riff_head, "RIFF", 4);
    wavehead.riffdata_len = LENGTH*RATE*CHANNELS*SIZE/8 + 36;
    memcpy(wavehead.wave_head, "WAVE", 4);
    memcpy(wavehead.fmt_head, "fmt ", 4);
    wavehead.fmtdata_len = 16;
    wavehead.format_tag = 1;
    wavehead.channels = CHANNELS;
    wavehead.samples_persec = RATE;
    wavehead.bytes_persec=RATE*CHANNELS*SIZE/8;
    wavehead.block_align=CHANNELS*SIZE/8;
    wavehead.bits_persec=SIZE;
    memcpy(wavehead.data_head, "data", 4);
    wavehead.data_len = LENGTH*RATE*CHANNELS*SIZE/8;
    /*以上wave 文件头赋值*/


    long loops;
    int rc,i = 0;
    int size;
    FILE *fp ;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val,val2;
    int dir;
    snd_pcm_uframes_t frames;
    char *buffer;

    system("rm -f sound.wav");//清除已有的wav录音文件

    int fd_f;
    if(( fd_f = open("./sound.wav", O_CREAT|O_RDWR,0777))==-1)//创建一个wave格式语音文件
    {
        perror("cannot creat the sound file");
    }

    if(write(fd_f, &wavehead, sizeof(wavehead))==-1)//写入wave文件的文件头
    {
        perror("write to sound'head wrong!!");
    }

    /* Open PCM device for recording (capture). */
    rc = snd_pcm_open(&handle, "default",
                      SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0)
    {
        fprintf(stderr,  "unable to open pcm device: %s/n",  snd_strerror(rc));
        exit(1);
    }
    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);
    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);
    /* Set the desired hardware parameters. */
    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params,
                                 SND_PCM_FORMAT_S16_LE);
    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, 1);
    /* 44100 bits/second sampling rate (CD quality) */
    val = RATE;
    snd_pcm_hw_params_set_rate_near(handle, params,  &val, &dir);
    /* Set period size to 32 frames. */
    frames = RSIZE/(SIZE/8);
    snd_pcm_hw_params_set_period_size_near(handle,  params, &frames, &dir);
    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0)
    {
        fprintf(stderr,  "unable to set hw parameters: %s/n",
                snd_strerror(rc));
        exit(1);
    }
    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params,  &frames, &dir);
    size = frames * SIZE /8; /* 2 bytes/sample, 2 channels */
    printf("size = %d\n",size);
    buffer = (char *) malloc(size);
    /* We want to loop for 5 seconds */
    snd_pcm_hw_params_get_period_time(params,  &val, &dir);
    for(i=0;i<(LENGTH*RATE*SIZE*CHANNELS/8)/RSIZE;i++)
    {

        rc = snd_pcm_readi(handle, buffer, frames);
        printf("%d\n",i++);
        if (rc == -EPIPE)
        {
            /* EPIPE means overrun */
            fprintf(stderr, "overrun occurred/n");
            snd_pcm_prepare(handle);
        }
        else if (rc < 0)
        {
            fprintf(stderr,
                    "error from read: %s/n",
                    snd_strerror(rc));
        }
        else if (rc != (int)frames)
        {
            fprintf(stderr, "short read, read %d frames/n", rc);
        }

        if(write(fd_f, buffer, size)==-1)
        {
            perror("write to sound wrong!!");
        }

        else printf("fwrite buffer success\n");
    }
    /******************打印参数*********************/
    snd_pcm_hw_params_get_channels(params, &val);
    printf("channels = %d\n", val);
    snd_pcm_hw_params_get_rate(params, &val, &dir);
    printf("rate = %d bps\n", val);
    snd_pcm_hw_params_get_period_time(params,
                                      &val, &dir);
    printf("period time = %d us\n", val);
    snd_pcm_hw_params_get_period_size(params,
                                      &frames, &dir);
    printf("period size = %d frames\n", (int)frames);
    snd_pcm_hw_params_get_buffer_time(params,
                                      &val, &dir);
    printf("buffer time = %d us\n", val);
    snd_pcm_hw_params_get_buffer_size(params,
                                      (snd_pcm_uframes_t *) &val);
    printf("buffer size = %d frames\n", val);
    snd_pcm_hw_params_get_periods(params, &val, &dir);
    printf("periods per buffer = %d frames\n", val);
    snd_pcm_hw_params_get_rate_numden(params,
                                      &val, &val2);
    printf("exact rate = %d/%d bps\n", val, val2);
    val = snd_pcm_hw_params_get_sbits(params);
    printf("significant bits = %d\n", val);
//snd_pcm_hw_params_get_tick_time(params,  &val, &dir);
    printf("tick time = %d us\n", val);
    val = snd_pcm_hw_params_is_batch(params);
    printf("is batch = %d\n", val);
    val = snd_pcm_hw_params_is_block_transfer(params);
    printf("is block transfer = %d\n", val);
    val = snd_pcm_hw_params_is_double(params);
    printf("is double = %d\n", val);
    val = snd_pcm_hw_params_is_half_duplex(params);
    printf("is half duplex = %d\n", val);
    val = snd_pcm_hw_params_is_joint_duplex(params);
    printf("is joint duplex = %d\n", val);
    val = snd_pcm_hw_params_can_overrange(params);
    printf("can overrange = %d\n", val);
    val = snd_pcm_hw_params_can_mmap_sample_resolution(params);
    printf("can mmap = %d\n", val);
    val = snd_pcm_hw_params_can_pause(params);
    printf("can pause = %d\n", val);
    val = snd_pcm_hw_params_can_resume(params);
    printf("can resume = %d\n", val);
    val = snd_pcm_hw_params_can_sync_start(params);
    printf("can sync start = %d\n", val);
    /*******************************************************************/
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    close(fd_f);
    free(buffer);
    return 0;
}
