#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include "../../common/image.h"
#include "../../common/bmp.h"
#include "../../common/k_means_compression.h"
#include "../../common/color_conversion.h"

#define INPUT_FILE "../../resource/bbb_sunflower_180p_30fps_2min.mp4"
#define OUTPUT_DIR "../../output/bbb_sunflower_compressed_frames/"
#define N_COLOR 16

static int frame_to_image_rgb(const AVFrame* src, image_t* dst);

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
    AVStream* input_stream = NULL;
    for ( unsigned int i = 0; i < format_context->nb_streams; i++)
    {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            input_stream = format_context->streams[i];
            break;
        }
    }
    CHECK_EXPR(input_stream, "Failed to find video stream");

    const AVCodec* decoder = avcodec_find_decoder(input_stream->codecpar->codec_id);
    CHECK_EXPR(decoder, "Failed to find decoder");
    AVCodecContext* decoder_context = avcodec_alloc_context3(decoder);
    CHECK_EXPR(decoder_context, "Failed to allocate decoder context");
    rc = avcodec_parameters_to_context(decoder_context, input_stream->codecpar);
    CHECK_EXPR(rc == 0, "Failed to copy decoder parameters");
    rc = avcodec_open2(decoder_context, decoder, NULL);
    CHECK_EXPR(rc == 0, "Failed to open decoder");

    AVRational frame_rate = av_guess_frame_rate(format_context, input_stream, NULL);

    AVFrame* frame = av_frame_alloc();
    CHECK_EXPR(frame, "Failed to allocate frame");
    AVPacket* packet = av_packet_alloc();
    CHECK_EXPR(packet, "Failed to allocate packet");
    image_t* image = NULL;
    color_palette_image_t* compressed_image = NULL;
    int n_frame = 0;
    while(av_read_frame(format_context, packet) >= 0)
    {
        if (packet->stream_index == input_stream->index)
        {
            rc = avcodec_send_packet(decoder_context, packet);
            av_packet_unref(packet);
            CHECK_EXPR(rc == 0, "Failed to send packet");
            while((rc = avcodec_receive_frame(decoder_context, frame)) >= 0)
            {
                if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
                {
                    break;
                }
                CHECK_EXPR(rc == 0, "Failed to receive frame");
                printf("Frame %d\n", n_frame);
                if (image == NULL)
                {
                    image = image_new(frame->width, frame->height);
                    CHECK_EXPR(image, "Failed to allocate image");
                }
                if (compressed_image == NULL)
                {
                    compressed_image = color_palette_image_new(N_COLOR, frame->width, frame->height);
                    CHECK_EXPR(compressed_image, "Failed to allocate compressed image");
                }
                {
                    rc = frame_to_image_rgb(frame, image);
                    CHECK_EXPR(rc == 0, "Failed to convert frame to image");

                    bgr_image_to_ycbcr(image, image);
                    int iterations = k_means_compression(image, 16, compressed_image, n_frame != 0);
                    CHECK_EXPR(iterations >= 0, "Failed to compress image");
                    paint_color_palette_image(compressed_image, image);
                    ycbcr_image_to_bgr(image, image);

                    char filename[PATH_MAX + 1] = {0};
                    snprintf(filename, PATH_MAX, "%s/%d.bmp", OUTPUT_DIR, n_frame);
                    dump_image_to_bmp(filename, image);
                }

                n_frame++;

                av_frame_unref(frame);
            }
        }
    }

    image_free(image);
    color_palette_image_free(compressed_image);
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&decoder_context);
    avformat_close_input(&format_context);
    avformat_free_context(format_context);

    return 0;
}

static int frame_to_image_rgb(const AVFrame* src, image_t* dst)
{
    AVFrame* frame = (AVFrame*)src;
    bool frame_is_allocated = false;
    if ((int)src->width != (int)dst->width || (int)src->height != (int)dst->height)
        return -1;
    /** Convert src to RGB24 */
    if (src->format != AV_PIX_FMT_RGB24)
    {
        frame_is_allocated = true;
        frame = av_frame_alloc();
        if (!frame)
            return -1;
        frame->format = AV_PIX_FMT_RGB24;
        frame->width = src->width;
        frame->height = src->height;
        int rc = av_image_alloc(
            frame->data, frame->linesize, frame->width, frame->height, frame->format, 32);
        if (rc < 0)
        {
            av_frame_free(&frame);
            return -1;
        }
        struct SwsContext* sws_context = sws_getContext(
            src->width, src->height, src->format,
            frame->width, frame->height, frame->format,
            SWS_BILINEAR, NULL, NULL, NULL);
        if (!sws_context)
        {
            av_freep(&frame->data[0]);
            av_frame_free(&frame);
            return -1;
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
            return -1;
        }
    }
    dst->color_space = COLOR_SPACE_BGR;
    for (int y = 0; y < frame->height; y++)
    {
        for (int x = 0; x < frame->width; x++)
        {
            int src_index = y * frame->linesize[0] + x * 3;
            dst->pixels[y * frame->width + x].bgr.r = frame->data[0][src_index];
            dst->pixels[y * frame->width + x].bgr.g = frame->data[0][src_index + 1];
            dst->pixels[y * frame->width + x].bgr.b = frame->data[0][src_index + 2];
        }
    }
    if (frame_is_allocated)
    {
        av_freep(&frame->data[0]);
        av_frame_free(&frame);
    }

    return 0;
}

