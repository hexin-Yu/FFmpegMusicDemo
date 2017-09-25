#include <jni.h>
#include <string>
#include <android/log.h>

extern "C"
{
//编码
#include "libavcodec/avcodec.h"
//封装格式处理
#include "libavformat/avformat.h"
//缩放处理
#include "libswscale/swscale.h"
//重采样
#include "libswresample/swresample.h"
}

#define LOG_TAG "pf"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define MAX_AUDIO_FRAME_SIZE 48000 * 4

extern "C"
{
JNIEXPORT void JNICALL
Java_com_pf_ffmpegmusicdemo_MusicPlayer_changeFile(JNIEnv *env, jobject instance, jstring input_,
                                                   jstring output_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    const char *output = env->GetStringUTFChars(output_, 0);

    av_register_all();
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    //打开音频文件
    if (avformat_open_input(&pFormatCtx, input, NULL, NULL) != 0) {
        LOGE("音频文件打开失败");
        return;
    }
    //获取文件输入信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("获取文件信息失败");
        return;
    }
    //获取音频流索引位置
    int i = 0, audio_stream_idx = -1;
    for (; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
            break;
        }
    }
    //获取解码器
    AVCodecContext *pCodecCtx = pFormatCtx->streams[audio_stream_idx]->codec;
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    //打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGE("打开解码器失败");
        return;
    }
    //压缩数据
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    //解压缩数据
    AVFrame *frame = av_frame_alloc();
    //frame->16bit 44100 PCM 统一音频采样格式与采样率
    SwrContext *swrContext = swr_alloc();
    //音频格式,重采样设置参数
    AVSampleFormat in_sample_fmt = pCodecCtx->sample_fmt;
    //输出采样格式
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    //输入采样率
    int in_sample_rate = pCodecCtx->sample_rate;
    //输出采样率
    int out_sample_rate = 44100;
    //输入声道布局
    uint64_t in_ch_layout = pCodecCtx->channel_layout;
    //输出声道布局
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
    //设置参数
    swr_alloc_set_opts(swrContext, out_ch_layout, out_sample_fmt, out_sample_rate, in_ch_layout,
                       in_sample_fmt, in_sample_rate, 0, NULL);
    //初始化
    swr_init(swrContext);
    int got_frame = 0;
    int ret;
    int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);
    LOGE("声道数量:%d", out_channel_nb);
    int count = 0;
    //设置音频缓冲区间 16bit 44100  PCM数据
    uint8_t *out_buffer = (uint8_t *) av_malloc(2 * 44100);
    //打开文件
    FILE *fp_pcm = fopen(output, "wb");
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        ret = avcodec_decode_audio4(pCodecCtx, frame, &got_frame, packet);
        LOGE("正在解码:%d", count++);
        if (ret < 0) {
            LOGE("解码完成");
        }
        //解码一帧
        if (got_frame > 0) {
            swr_convert(swrContext, &out_buffer, 2 * 44100, (const uint8_t **) frame->data,
                        frame->nb_samples);
            int out_buffer_size = av_samples_get_buffer_size(NULL, out_channel_nb,
                                                             frame->nb_samples, out_sample_fmt, 1);
            fwrite(out_buffer, 1, out_buffer_size, fp_pcm);
        }
    }
    //释放资源
    fclose(fp_pcm);
    av_frame_free(&frame);
    av_free(out_buffer);
    swr_free(&swrContext);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    env->ReleaseStringUTFChars(input_, input);
    env->ReleaseStringUTFChars(output_, output);
}

JNIEXPORT void JNICALL
Java_com_pf_ffmpegmusicdemo_MusicPlayer_play(JNIEnv *env, jobject instance, jstring input_) {
    const char *input = env->GetStringUTFChars(input_, 0);

    av_register_all();
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    //打开音频文件
    if (avformat_open_input(&pFormatCtx, input, NULL, NULL) != 0) {
        LOGE("音频文件打开失败");
        return;
    }
    //获取文件输入信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("获取文件信息失败");
        return;
    }
    //获取音频流索引位置
    int i, audio_stream_idx = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
            break;
        }
    }
    //获取解码器
    AVCodecContext *pCodecCtx = pFormatCtx->streams[audio_stream_idx]->codec;
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    //打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGE("打开解码器失败");
        return;
    }
    //压缩数据
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    //解压缩数据
    AVFrame *frame = av_frame_alloc();
    //frame->16bit 44100 PCM 统一音频采样格式与采样率
    SwrContext *swrContext = swr_alloc();
    //音频格式,重采样设置参数
    AVSampleFormat in_sample_fmt = pCodecCtx->sample_fmt;
    //输出采样格式
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    //输入采样率
    int in_sample_rate = pCodecCtx->sample_rate;
    //输出采样率
    int out_sample_rate = pCodecCtx->sample_rate;
    //输入声道布局
    uint64_t in_ch_layout = pCodecCtx->channel_layout;
    //输出声道布局
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
    //设置参数
    swr_alloc_set_opts(swrContext, out_ch_layout, out_sample_fmt, out_sample_rate, in_ch_layout,
                       in_sample_fmt, in_sample_rate, 0, NULL);
    //初始化
    swr_init(swrContext);
    int got_frame = 0;
    int ret;
    int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);
    LOGE("声道数量:%d", out_channel_nb);

    //设置音频缓冲区间 16bit 44100  PCM数据
    uint8_t *out_buffer = (uint8_t *) av_malloc(2 * 44100);

    //获取class
    jclass music_player = env->GetObjectClass(instance);
    //得到createAudio方法
    jmethodID createAudioID = env->GetMethodID(music_player, "createAudio", "(II)V");
    //调用createAudio
    env->CallVoidMethod(instance, createAudioID, 44100, out_channel_nb);
    jmethodID playTrackID = env->GetMethodID(music_player, "playTrack", "([BI)V");

    int count = 0;
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == audio_stream_idx) {
            //解码
            avcodec_decode_audio4(pCodecCtx, frame, &got_frame, packet);
            if (got_frame) {
                LOGE("解码:%d", count++);
                swr_convert(swrContext, &out_buffer, 44100 * 2, (const uint8_t **) frame->data,
                            frame->nb_samples);
                //缓冲区的大小
                int size = av_samples_get_buffer_size(NULL, out_channel_nb, frame->nb_samples,
                                                      AV_SAMPLE_FMT_S16P, 1);
                jbyteArray audio_sample_array = env->NewByteArray(size);
                env->SetByteArrayRegion(audio_sample_array, 0, size, (const jbyte *) out_buffer);
                env->CallVoidMethod(instance, playTrackID, audio_sample_array, size);
                env->DeleteLocalRef(audio_sample_array);
            }
        }
    }
    av_frame_free(&frame);
    swr_free(&swrContext);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    env->ReleaseStringUTFChars(input_, input);
}
}