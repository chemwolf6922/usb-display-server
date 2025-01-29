#define _GNU_SOURCE

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../../common/image.h"
#include "../../common/k_means_compression.h"
#include "../../common/color_conversion.h"
#include "../server/config.h"


static int resize_frame_to_bgr_image(const AVFrame* src, image_t* dst);
static uint64_t now_us();

#define CHECK_EXPR(expr, message) \
do { \
    if (!(expr)) \
    { \
        fprintf(stderr, "[%d] %s: %s\n", __LINE__, #expr, message); \
        return 1; \
    } \
} while(0)

int main(int argc, char* const* argv)
{
    const char* input_file = NULL;
    const char* server_path = DEFAULT_SOCK_PATH;
    int opt = -1;
    while ((opt = getopt(argc, argv, "s:i:")) != -1)
    {
        switch (opt)
        {
        case 's':
            server_path = optarg;
            break;
        case 'i':
            input_file = optarg;
            break;
        default:
            break;
        }
    }
    if (input_file == NULL)
    {
        fprintf(stderr, "Usage: %s -i <input file> [-s <server path>]\n", argv[0]);
        return 1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK_EXPR(fd != -1, "Failed to create socket");
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, server_path, sizeof(addr.sun_path) - 1);
    size_t addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
    if (addr.sun_path[0] == '@')
    {
        addr.sun_path[0] = '\0';
    }
    int rc = connect(fd, (struct sockaddr*)&addr, addr_len);
    CHECK_EXPR(rc == 0, "Failed to connect to server");

    AVFormatContext* format_context = NULL;
    rc = avformat_open_input(&format_context, input_file, NULL, NULL);
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
    uint64_t frame_time_us = av_q2d(av_inv_q(frame_rate)) * 1000000;
    printf("Frame rate: %d/%d\n", frame_rate.num, frame_rate.den);
    printf("Frame time: %lu us\n", frame_time_us);

    AVFrame* frame = av_frame_alloc();
    CHECK_EXPR(frame, "Failed to allocate frame");
    AVPacket* packet = av_packet_alloc();
    CHECK_EXPR(packet, "Failed to allocate packet");
    image_t* image = NULL;
    color_palette_image_t* compressed_image = NULL;
    int n_frame = 0;
    uint64_t last_frame_time = now_us();
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
                if (image == NULL)
                {
                    image = image_new(frame->width, frame->height);
                    CHECK_EXPR(image, "Failed to allocate image");
                }
                if (compressed_image == NULL)
                {
                    compressed_image = color_palette_image_new(CONST_N_COLOR, frame->width, frame->height);
                    CHECK_EXPR(compressed_image, "Failed to allocate compressed image");
                }
                {
                    rc = resize_frame_to_bgr_image(frame, image);
                    CHECK_EXPR(rc == 0, "Failed to convert frame to image");
                    rc = (int)send(fd, image->pixels, CONST_FB_SIZE, SOCK_NONBLOCK);
                    CHECK_EXPR(rc == CONST_FB_SIZE, "Failed to write to server");
                }
                n_frame++;
                av_frame_unref(frame);
                uint64_t now = now_us();
                if (last_frame_time == 0)
                {
                    last_frame_time = now;
                }
                if (now - last_frame_time < frame_time_us)
                {
                    usleep(frame_time_us - (now - last_frame_time));
                }
                now = now_us();
                last_frame_time = now_us();
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

static int resize_frame_to_bgr_image(const AVFrame* src, image_t* dst)
{
    /** Convert src to RGB24  and resize to the target bmp size */
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return -1;
    frame->format = AV_PIX_FMT_RGB24;
    frame->width = CONST_SCREEN_WIDTH;
    frame->height = CONST_SCREEN_HEIGHT;
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
    av_freep(&frame->data[0]);
    av_frame_free(&frame);
    return 0;
}

static uint64_t now_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

