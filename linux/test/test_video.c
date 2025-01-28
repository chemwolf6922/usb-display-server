#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdio.h>
#include <stdbool.h>
#include "../../common/image.h"
#include "../../common/bmp.h"

#define INPUT_FILE "../../resource/bbb_sunflower_180p_30fps_2min.mp4"
#define OUTPUT_FILE "../../output/bbb_sunflower_180p_30fps_2min.mp4"

static int save_frame_as_bmp(const AVFrame* frame, const char* filename);
static image_t* frame_to_image_rgb(const AVFrame* src);

#define CHECK_EXPR(expr, message) \
do { \
    if (!(expr)) \
    { \
        fprintf(stderr, "[%d] %s: %s\n", __LINE__, #expr, message); \
        return 1; \
    } \
} while(0)

int main(int argc, char const *argv[])
{
    AVFormatContext* format_context = NULL;
    int rc = avformat_open_input(&format_context, INPUT_FILE, NULL, NULL);
    CHECK_EXPR(rc == 0, "Failed to open input file");
    rc = avformat_find_stream_info(format_context, NULL);
    CHECK_EXPR(rc == 0, "Failed to find stream info");
    AVStream* stream = NULL;
    for ( unsigned int i = 0; i < format_context->nb_streams; i++)
    {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            stream = format_context->streams[i];
            break;
        }
    }
    CHECK_EXPR(stream, "Failed to find video stream");
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    CHECK_EXPR(codec, "Failed to find decoder");
    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    CHECK_EXPR(codec_context, "Failed to allocate codec context");
    rc = avcodec_parameters_to_context(codec_context, stream->codecpar);
    CHECK_EXPR(rc == 0, "Failed to copy codec parameters");
    rc = avcodec_open2(codec_context, codec, NULL);
    CHECK_EXPR(rc == 0, "Failed to open codec");

    AVFrame* frame = av_frame_alloc();
    CHECK_EXPR(frame, "Failed to allocate frame");
    AVPacket* packet = av_packet_alloc();
    CHECK_EXPR(packet, "Failed to allocate packet");
    while(av_read_frame(format_context, packet) >= 0)
    {
        if (packet->stream_index == stream->index)
        {
            rc = avcodec_send_packet(codec_context, packet);
            CHECK_EXPR(rc == 0, "Failed to send packet");
            rc = avcodec_receive_frame(codec_context, frame);
            if (rc == AVERROR(EAGAIN))
            {
                av_packet_unref(packet);
                continue;
            }
            else if (rc == AVERROR_EOF)
            {
                break;
            }
            CHECK_EXPR(rc == 0, "Failed to receive frame");

            printf("Frame %"PRIi64": %dx%d\n", frame->pts, frame->width, frame->height);
            rc = save_frame_as_bmp(frame, "../../output/frame.bmp");
            CHECK_EXPR(rc == 0, "Failed to save frame as bmp");
            av_frame_unref(frame);
            break;
        }
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    avformat_free_context(format_context);

    return 0;
}

static int save_frame_as_bmp(const AVFrame* src, const char* filename)
{
    image_t* image = frame_to_image_rgb(src);
    if (!image)
    {
        return -1;
    }
    dump_image_to_bmp(filename, image);
    image_free(image);
    return 0;
}

static image_t* frame_to_image_rgb(const AVFrame* src)
{
    AVFrame* frame = (AVFrame*)src;
    bool frame_is_allocated = false;
    /** Convert src to RGB24 */
    if (src->format != AV_PIX_FMT_RGB24)
    {
        frame_is_allocated = true;
        frame = av_frame_alloc();
        if (!frame)
            return NULL;
        frame->format = AV_PIX_FMT_RGB24;
        frame->width = src->width;
        frame->height = src->height;
        int rc = av_image_alloc(
            frame->data, frame->linesize, frame->width, frame->height, frame->format, 32);
        if (rc < 0)
        {
            av_frame_free(&frame);
            return NULL;
        }
        struct SwsContext* sws_context = sws_getContext(
            src->width, src->height, src->format,
            frame->width, frame->height, frame->format,
            SWS_BILINEAR, NULL, NULL, NULL);
        if (!sws_context)
        {
            av_freep(&frame->data[0]);
            av_frame_free(&frame);
            return NULL;
        }
        rc = sws_scale(
            sws_context,
            (const uint8_t* const*)(src->data), src->linesize, 0, src->height,
            frame->data, frame->linesize);
        sws_freeContext(sws_context);
        if (rc < 0)
        {
            av_freep(&frame->data[0]);
            av_frame_free(&frame);
            return NULL;
        }
    }

    image_t* image = image_new(frame->width, frame->height);
    if (!image)
    {
        if (frame_is_allocated)
        {
            av_freep(&frame->data[0]);
            av_frame_free(&frame);
        }
        return NULL;
    }
    image->color_space = COLOR_SPACE_BGR;
    for (int y = 0; y < frame->height; y++)
    {
        for (int x = 0; x < frame->width; x++)
        {
            int src_index = y * frame->linesize[0] + x * 3;
            image->pixels[y * frame->width + x].bgr.r = frame->data[0][src_index];
            image->pixels[y * frame->width + x].bgr.g = frame->data[0][src_index + 1];
            image->pixels[y * frame->width + x].bgr.b = frame->data[0][src_index + 2];
        }
    }
    if (frame_is_allocated)
    {
        av_freep(&frame->data[0]);
        av_frame_free(&frame);
    }

    return image;
}
