/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2025 Mr-Ojii

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef miniaudio_ffmpeg_h
#define miniaudio_ffmpeg_h

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MA_NO_FFMPEG)
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

// queue
typedef struct ma_ffmpeg_queue_node
{
    void* data;
    size_t size;
    struct ma_ffmpeg_queue_node* next;
} ma_ffmpeg_queue_node;

typedef struct
{
    size_t size;
    size_t cursor;
    ma_ffmpeg_queue_node* front;
    ma_ffmpeg_queue_node* rear;
} ma_ffmpeg_queue;

static void ma_ffmpeg_queue_init(ma_ffmpeg_queue* queue);
static void ma_ffmpeg_queue_clear(ma_ffmpeg_queue* queue);
static ma_result ma_ffmpeg_queue_enqueue(ma_ffmpeg_queue* queue, void* data, size_t size);
static ma_result ma_ffmpeg_queue_read(ma_ffmpeg_queue* queue, void* data, size_t size, size_t* readSize);
#endif // !MA_NO_FFMPEG


typedef struct
{
    ma_data_source_base ds;
    ma_read_proc onRead;
    ma_seek_proc onSeek;
    ma_tell_proc onTell;
    void* pReadSeekTellUserData;
    ma_format format;
#if !defined(MA_NO_FFMPEG)
    uint8_t bitsPerSample;
    enum AVSampleFormat avFormat;
    AVFormatContext* formatCtx;
    AVCodecContext* codecCtx;
    AVStream* stream;
    AVFrame* frame;
    AVPacket* packet;
    SwrContext* swrCtx;
    uint8_t* swrBuf;
    uint64_t cursor;
    ma_ffmpeg_queue queue;
#endif
} ma_ffmpeg;

MA_API ma_result ma_ffmpeg_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_ffmpeg* pFFmpeg);
MA_API void ma_ffmpeg_uninit(ma_ffmpeg* pFFmpeg, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_ffmpeg_read_pcm_frames(ma_ffmpeg* pFFmpeg, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_ffmpeg_seek_to_pcm_frame(ma_ffmpeg* pFFmpeg, ma_uint64 frameIndex);
MA_API ma_result ma_ffmpeg_get_data_format(ma_ffmpeg* pFFmpeg, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_ffmpeg_get_cursor_in_pcm_frames(ma_ffmpeg* pFFmpeg, ma_uint64* pCursor);
MA_API ma_result ma_ffmpeg_get_length_in_pcm_frames(ma_ffmpeg* pFFmpeg, ma_uint64* pLength);

#ifdef __cplusplus
}
#endif
#endif // miniaudio_ffmpeg_h


#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)


static ma_result ma_ffmpeg_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_ffmpeg_read_pcm_frames((ma_ffmpeg*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_ffmpeg_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_ffmpeg_seek_to_pcm_frame((ma_ffmpeg*)pDataSource, frameIndex);
}

static ma_result ma_ffmpeg_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_ffmpeg_get_data_format((ma_ffmpeg*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_ffmpeg_ds_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_ffmpeg_get_cursor_in_pcm_frames((ma_ffmpeg*)pDataSource, pCursor);
}

static ma_result ma_ffmpeg_ds_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_ffmpeg_get_length_in_pcm_frames((ma_ffmpeg*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_ffmpeg_ds_vtable =
{
    ma_ffmpeg_ds_read,
    ma_ffmpeg_ds_seek,
    ma_ffmpeg_ds_get_data_format,
    ma_ffmpeg_ds_get_cursor,
    ma_ffmpeg_ds_get_length
};

#if !defined(MA_NO_FFMPEG)

// avio callback
static int ma_ffmpeg_avio_callback__read(void* opaque, uint8_t* buf, int buf_size) {
    ma_ffmpeg* pFFmpeg = (ma_ffmpeg*)opaque;
    ma_result result;
    size_t bytesToRead;

    if (!pFFmpeg->onRead || !buf_size) {
        return -1;
    }

    result = pFFmpeg->onRead(pFFmpeg->pReadSeekTellUserData, buf, buf_size, &bytesToRead);
    if (result != MA_SUCCESS) {
        return 0;
    }

    return bytesToRead;
}

static int64_t ma_ffmpeg_avio_callback__seek(void* opaque, int64_t offset, int whence) {
    ma_ffmpeg* pFFmpeg = (ma_ffmpeg*)opaque;
    ma_result result;
    ma_seek_origin origin;
    ma_int64 cursor;

    if (!pFFmpeg->onSeek || !pFFmpeg->onTell)
        return AVERROR(EINVAL);

    if (whence == SEEK_SET) {
        origin = ma_seek_origin_start;
    } else if (whence == SEEK_END) {
        origin = ma_seek_origin_end;
    } else if (whence == SEEK_CUR) {
        origin = ma_seek_origin_current;
    } else {
        return AVERROR(ENOSYS);
    }

    result = pFFmpeg->onSeek(pFFmpeg->pReadSeekTellUserData, offset, origin);
    if (result != MA_SUCCESS) {
        return AVERROR(EIO);
    }

    result = pFFmpeg->onTell(pFFmpeg->pReadSeekTellUserData, &cursor);
    if (result != MA_SUCCESS) {
        return AVERROR(EIO);
    }

    return cursor;
}
// avio callback

// queue
static void ma_ffmpeg_queue_init(ma_ffmpeg_queue* queue) {
    queue->size = 0;
    queue->cursor = 0;
    queue->front = NULL;
    queue->rear = NULL;
}

static void ma_ffmpeg_queue_clear(ma_ffmpeg_queue* queue) {
    ma_ffmpeg_queue_node* node;

    while(queue->front) {
        node = queue->front;
        queue->front = node->next;
        free(node->data);
        free(node);
    }

    queue->size = 0;
    queue->cursor = 0;
    queue->front = NULL;
    queue->rear = NULL;
}

static ma_result ma_ffmpeg_queue_enqueue(ma_ffmpeg_queue* queue, void* data, size_t size) {
    ma_ffmpeg_queue_node* node = (ma_ffmpeg_queue_node*)malloc(sizeof(ma_ffmpeg_queue_node));

    if (!node) {
        return MA_OUT_OF_MEMORY;
    }
    node->data = malloc(size);
    if (!node->data) {
        free(node);
        return MA_OUT_OF_MEMORY;
    }

    memcpy(node->data, data, size);
    node->size = size;
    node->next = NULL;

    if (!queue->rear) {
        queue->front = node;
        queue->rear = node;
    } else {
        queue->rear->next = node;
        queue->rear = node;
    }

    queue->size += size;

    return MA_SUCCESS;
}

static ma_result ma_ffmpeg_queue_read(ma_ffmpeg_queue* queue, void* data, size_t size, size_t* readSize) {
    size_t readed = 0;
    if (readSize)
        *readSize = 0;

    if (!queue->front)
        return MA_ERROR;

    while(queue->front) {
        ma_ffmpeg_queue_node* node = queue->front;

        if ((node->size - queue->cursor) + readed > size) {
            // 途中まで読む
            size_t rSize = size - readed;

            if (data)
                memcpy((ma_uint8*)data + readed, (ma_uint8*)node->data + queue->cursor, rSize);

            queue->size -= rSize;
            queue->cursor = queue->cursor + rSize;
            readed += rSize;

            break;
        } else {
            size_t rSize = node->size - queue->cursor;
            // 完全に読んで、抜ける

            if (data)
                memcpy((ma_uint8*)data + readed, (ma_uint8*)node->data + queue->cursor, rSize);

            queue->front = node->next;
            queue->size -= rSize;
            queue->cursor = 0;
            readed += rSize;

            free(node->data);
            free(node);

            if (queue->front == NULL)
                queue->rear = NULL;
        }
    }

    if (readSize)
        *readSize = readed;

    return MA_SUCCESS;
}
// queue

static ma_result decode_one_cycle(ma_ffmpeg* pFFmpeg) {
    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    int ret = AVERROR_UNKNOWN;
    if ((ret = av_read_frame(pFFmpeg->formatCtx, pFFmpeg->packet)) < 0) {
        if (ret == AVERROR_EOF) {
            return MA_AT_END;
        }
        return MA_ERROR;
    }
    if (pFFmpeg->packet->stream_index == pFFmpeg->stream->index) {
        if (avcodec_send_packet(pFFmpeg->codecCtx, pFFmpeg->packet) < 0) {
            // エラーだが、継続
        }
        if ((ret = avcodec_receive_frame(pFFmpeg->codecCtx, pFFmpeg->frame)) < 0) {
            if (ret == AVERROR_EOF) {
                return MA_AT_END;
            }
            else if (ret != AVERROR(EAGAIN)) {
                return MA_ERROR;
            }
        } else {
            // PTSから補正をかける
            if (pFFmpeg->frame->pts != AV_NOPTS_VALUE)
                pFFmpeg->cursor = (pFFmpeg->frame->pts * pFFmpeg->codecCtx->sample_rate * pFFmpeg->stream->time_base.num) / pFFmpeg->stream->time_base.den;
            pFFmpeg->cursor += pFFmpeg->frame->nb_samples;
            if (pFFmpeg->frame->format != pFFmpeg->avFormat) {
                if (pFFmpeg->swrCtx == NULL) {
                    pFFmpeg->swrCtx = swr_alloc();
                    if (pFFmpeg->swrCtx == NULL)
                        return MA_ERROR;
                    av_opt_set_chlayout(pFFmpeg->swrCtx, "in_chlayout", &pFFmpeg->frame->ch_layout, 0);
                    av_opt_set_chlayout(pFFmpeg->swrCtx, "out_chlayout", &pFFmpeg->frame->ch_layout, 0);
                    av_opt_set_int(pFFmpeg->swrCtx, "in_sample_rate", pFFmpeg->frame->sample_rate, 0);
                    av_opt_set_int(pFFmpeg->swrCtx, "out_sample_rate", pFFmpeg->frame->sample_rate, 0);
                    av_opt_set_sample_fmt(pFFmpeg->swrCtx, "in_sample_fmt", (enum AVSampleFormat)pFFmpeg->frame->format, 0);
                    av_opt_set_sample_fmt(pFFmpeg->swrCtx, "out_sample_fmt", pFFmpeg->avFormat, 0);
                    if (swr_init(pFFmpeg->swrCtx) < 0) {
                        return MA_ERROR;
                    }
                    int swr_buf_len =  av_samples_get_buffer_size(NULL, pFFmpeg->frame->ch_layout.nb_channels, pFFmpeg->frame->sample_rate, pFFmpeg->avFormat, 1);
                    pFFmpeg->swrBuf = (uint8_t*)av_malloc(swr_buf_len);
                }

                if (swr_convert(pFFmpeg->swrCtx, &pFFmpeg->swrBuf, pFFmpeg->frame->nb_samples,
                                    (const uint8_t* const*)pFFmpeg->frame->extended_data, pFFmpeg->frame->nb_samples) < 0) {
                    return MA_ERROR;
                }
                size_t count = pFFmpeg->frame->nb_samples * pFFmpeg->bitsPerSample * pFFmpeg->frame->ch_layout.nb_channels;
                ma_ffmpeg_queue_enqueue(&pFFmpeg->queue, pFFmpeg->swrBuf, count);
            } else {
                size_t count = pFFmpeg->frame->nb_samples * pFFmpeg->bitsPerSample * pFFmpeg->frame->ch_layout.nb_channels;
                ma_ffmpeg_queue_enqueue(&pFFmpeg->queue, pFFmpeg->frame->extended_data[0], count);
            }
        }
    }
    av_packet_unref(pFFmpeg->packet);
    return MA_SUCCESS;
}

#endif // !MA_NO_FFMPEG

static ma_result ma_ffmpeg_init_internal(const ma_decoding_backend_config *pConfig, ma_ffmpeg* pFFmpeg) {
    ma_result result;
    ma_data_source_config dataSourceConfig;

    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pFFmpeg);
    pFFmpeg->format = ma_format_f32;

    #if !defined(MA_NO_FFMPEG)
    {
        pFFmpeg->avFormat = AV_SAMPLE_FMT_FLT;
        pFFmpeg->bitsPerSample = 4;

        if (pConfig != NULL) {
            switch(pConfig->preferredFormat) {
                case ma_format_u8:
                    pFFmpeg->format = ma_format_u8;
                    pFFmpeg->avFormat = AV_SAMPLE_FMT_U8;
                    pFFmpeg->bitsPerSample = 1;
                    break;
                case ma_format_s16:
                    pFFmpeg->format = ma_format_s16;
                    pFFmpeg->avFormat = AV_SAMPLE_FMT_S16;
                    pFFmpeg->bitsPerSample = 2;
                    break;
                case ma_format_s32:
                    pFFmpeg->format = ma_format_s32;
                    pFFmpeg->avFormat = AV_SAMPLE_FMT_S32;
                    pFFmpeg->bitsPerSample = 4;
                    break;
                default:
                    break;
            }
        }
    }
    #endif

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_ffmpeg_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pFFmpeg->ds);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}


MA_API ma_result ma_ffmpeg_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_ffmpeg *pFFmpeg) {
    ma_result result;

    (void)pAllocationCallbacks;
    (void)pConfig;

    result = ma_ffmpeg_init_internal(pConfig, pFFmpeg);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (onRead == NULL || onSeek == NULL || onTell == NULL) {
        return MA_INVALID_ARGS;
    }

    pFFmpeg->onRead = onRead;
    pFFmpeg->onSeek = onSeek;
    pFFmpeg->onTell = onTell;
    pFFmpeg->pReadSeekTellUserData = pReadSeekTellUserData;

    #if !defined(MA_NO_FFMPEG)
    {
        ma_ffmpeg_queue_init(&pFFmpeg->queue); // 一応
        pFFmpeg->formatCtx = avformat_alloc_context();
        if (!pFFmpeg->formatCtx) {
            return MA_ERROR;
        }
        const size_t buf_size = 1024;
        unsigned char* buffer = (unsigned char*)av_malloc(buf_size);
        if (!buffer) {
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        AVIOContext* avio_ctx = avio_alloc_context(buffer, buf_size, 0, pFFmpeg, ma_ffmpeg_avio_callback__read, NULL, ma_ffmpeg_avio_callback__seek);
        if (!avio_ctx) {
            av_free(buffer);
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        pFFmpeg->formatCtx->pb = avio_ctx;
        if (avformat_open_input(&pFFmpeg->formatCtx, NULL, NULL, NULL) < 0) {
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        if (avformat_find_stream_info(pFFmpeg->formatCtx, NULL) < 0) {
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }

        for (unsigned int i = 0; i < pFFmpeg->formatCtx->nb_streams; i++) {
            if (pFFmpeg->formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                pFFmpeg->stream = pFFmpeg->formatCtx->streams[i];
                break;
            }
        }
        if (!pFFmpeg->stream) {
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        const AVCodec* codec = avcodec_find_decoder(pFFmpeg->stream->codecpar->codec_id);
        if (!codec) {
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        pFFmpeg->codecCtx = avcodec_alloc_context3(codec);
        if (!pFFmpeg->codecCtx) {
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        if (avcodec_parameters_to_context(pFFmpeg->codecCtx, pFFmpeg->stream->codecpar) < 0) {
            avformat_close_input(&pFFmpeg->formatCtx);
            avcodec_free_context(&pFFmpeg->codecCtx);
            return MA_ERROR;
        }
        if (avcodec_open2(pFFmpeg->codecCtx, codec, NULL) < 0) {
            avformat_close_input(&pFFmpeg->formatCtx);
            avcodec_free_context(&pFFmpeg->codecCtx);
            return MA_ERROR;
        }
        pFFmpeg->frame = av_frame_alloc();
        if (!pFFmpeg->frame) {
            avformat_close_input(&pFFmpeg->formatCtx);
            avcodec_free_context(&pFFmpeg->codecCtx);
            return MA_ERROR;
        }
        pFFmpeg->packet = av_packet_alloc();
        if (!pFFmpeg->packet) {
            avformat_close_input(&pFFmpeg->formatCtx);
            avcodec_free_context(&pFFmpeg->codecCtx);
            av_frame_free(&pFFmpeg->frame);
            return MA_ERROR;
        }

        return MA_SUCCESS;
    }
    #else
    {
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}


MA_API void ma_ffmpeg_uninit(ma_ffmpeg *pFFmpeg, const ma_allocation_callbacks *pAllocationCallbacks) {
    if (!pFFmpeg) {
        return;
    }

    (void)pAllocationCallbacks;

    #if !defined(MA_NO_FFMPEG)
    {
        av_free(pFFmpeg->swrBuf);
        swr_free(&pFFmpeg->swrCtx);
        av_packet_unref(pFFmpeg->packet);
        av_frame_free(&pFFmpeg->frame);
        avcodec_free_context(&pFFmpeg->codecCtx);
        avformat_close_input(&pFFmpeg->formatCtx); // AVIOContextもこれで解放される
        ma_ffmpeg_queue_clear(&pFFmpeg->queue);
    }
    #else
    {
        MA_ASSERT(MA_FALSE);
    }
    #endif

    ma_data_source_uninit(&pFFmpeg->ds);
}


MA_API ma_result ma_ffmpeg_read_pcm_frames(ma_ffmpeg *pFFmpeg, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead) {
    if (pFramesRead) {
        *pFramesRead = 0;
    }
    if (!frameCount || !pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FFMPEG)
    {
        size_t bytesCount = frameCount * pFFmpeg->bitsPerSample * pFFmpeg->codecCtx->ch_layout.nb_channels;
        ma_result ret;
        while (pFFmpeg->queue.size < bytesCount) {
            ret = decode_one_cycle(pFFmpeg);
            if (ret == MA_AT_END)
                break;
            if (ret != MA_SUCCESS)
                return ret;
        }

        size_t readBytes;
        ma_ffmpeg_queue_read(&pFFmpeg->queue, pFramesOut, bytesCount, &readBytes);
        *pFramesRead = readBytes / pFFmpeg->bitsPerSample / pFFmpeg->codecCtx->ch_layout.nb_channels;

        return ret;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);

        (void)pFramesOut;
        (void)frameCount;
        (void)pFramesRead;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}



MA_API ma_result ma_ffmpeg_seek_to_pcm_frame(ma_ffmpeg *pFFmpeg, ma_uint64 frameIndex) {
    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FFMPEG)
    {
        ma_uint64 length;
        ma_ffmpeg_get_length_in_pcm_frames(pFFmpeg, &length);
        if (!pFFmpeg->formatCtx || !pFFmpeg->codecCtx || !pFFmpeg->stream || length < frameIndex) {
            return MA_INVALID_ARGS;
        }
        AVRational avr = { 1, pFFmpeg->codecCtx->sample_rate };
        int64_t timestamp = av_rescale_q(frameIndex, avr, pFFmpeg->stream->time_base);
        if (av_seek_frame(pFFmpeg->formatCtx, pFFmpeg->stream->index, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
            return MA_ERROR;
        }

        AVCodecContext* ctx = avcodec_alloc_context3(pFFmpeg->codecCtx->codec);
        if (!ctx) {
            avcodec_flush_buffers(pFFmpeg->codecCtx);
            goto ma_ffmpeg_recreate_context_failed;
        }
        if (avcodec_parameters_to_context(ctx, pFFmpeg->stream->codecpar) < 0) {
            avcodec_free_context(&ctx);
            avcodec_flush_buffers(pFFmpeg->codecCtx);
            goto ma_ffmpeg_recreate_context_failed;
        }
        if (avcodec_open2(ctx, pFFmpeg->codecCtx->codec, NULL) < 0) {
            avcodec_free_context(&ctx);
            avcodec_flush_buffers(pFFmpeg->codecCtx);
            goto ma_ffmpeg_recreate_context_failed;
        }
        avcodec_free_context(&pFFmpeg->codecCtx);
        pFFmpeg->codecCtx = ctx;

        ma_ffmpeg_recreate_context_failed:

        ma_ffmpeg_queue_clear(&pFFmpeg->queue);

        // AVSEEK_FLAG_BACKWARDをおいたので、少し前サンプルまで拾ってしまう
        // 一回デコードして、queueからいらない分を抜いて補正する
        do
        {
            decode_one_cycle(pFFmpeg);
        } while(pFFmpeg->cursor < frameIndex);
        int popBytes = (frameIndex - (pFFmpeg->cursor - (pFFmpeg->queue.size / pFFmpeg->bitsPerSample / pFFmpeg->codecCtx->ch_layout.nb_channels))) * pFFmpeg->bitsPerSample * pFFmpeg->codecCtx->ch_layout.nb_channels;
        ma_ffmpeg_queue_read(&pFFmpeg->queue, NULL, popBytes, NULL);

        return MA_SUCCESS;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);

        (void)frameIndex;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}



MA_API ma_result ma_ffmpeg_get_data_format(ma_ffmpeg *pFFmpeg, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap) {
    if (pFormat) {
        *pFormat = ma_format_unknown;
    }
    if (pChannels) {
        *pChannels = 0;
    }
    if (pSampleRate) {
        *pSampleRate = 0;
    }
    if (pChannelMap) {
        MA_ZERO_MEMORY(pChannelMap, sizeof(*pChannelMap) * channelMapCap);
    }

    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    if (pFormat) {
        *pFormat = pFFmpeg->format;
    }

    #if !defined(MA_NO_FFMPEG)
    {
        if (!pFFmpeg->codecCtx) {
            return MA_INVALID_ARGS;
        }
        if (pChannels) {
            *pChannels = pFFmpeg->codecCtx->ch_layout.nb_channels;
        }
        if (pSampleRate) {
            *pSampleRate = pFFmpeg->codecCtx->sample_rate;
        }
        if (pChannelMap) {
            ma_channel_map_init_standard(ma_standard_channel_map_vorbis, pChannelMap, channelMapCap, pFFmpeg->codecCtx->ch_layout.nb_channels);
        }
        return MA_SUCCESS;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif

}


MA_API ma_result ma_ffmpeg_get_cursor_in_pcm_frames(ma_ffmpeg* pFFmpeg, ma_uint64* pCursor) {
    if (!pCursor) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;

    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FFMPEG)
    {
        if (!pFFmpeg->codecCtx) {
            return MA_INVALID_ARGS;
        }

        // queueに積んである分だけcursorがズレてる
        *pCursor = pFFmpeg->cursor - (pFFmpeg->queue.size / pFFmpeg->bitsPerSample / pFFmpeg->codecCtx->ch_layout.nb_channels);

        return MA_SUCCESS;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif

}


MA_API ma_result ma_ffmpeg_get_length_in_pcm_frames(ma_ffmpeg *pFFmpeg, ma_uint64 *pLength) {
    if (!pLength) {
        return MA_INVALID_ARGS;
    }

    *pLength = 0;

    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FFMPEG)
    {
        if (!pFFmpeg->stream) {
            return MA_INVALID_ARGS;
        }

        if (pFFmpeg->stream->nb_frames) {
            *pLength = pFFmpeg->stream->nb_frames;
            return MA_SUCCESS;
        }
        if (pFFmpeg->stream->duration != AV_NOPTS_VALUE && pFFmpeg->stream->codecpar->sample_rate != 0) {
            *pLength =
                (pFFmpeg->stream->duration * pFFmpeg->stream->codecpar->sample_rate * pFFmpeg->stream->time_base.num) /
                pFFmpeg->stream->time_base.den;
            return MA_SUCCESS;
        }

        return MA_ERROR;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

#endif // MA_IMPLEMENTATION

