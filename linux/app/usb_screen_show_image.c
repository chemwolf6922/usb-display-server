#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include "../server/config.h"
#include "../../common/image.h"

enum
{
    MODE_STRETCH,
    MODE_FIT,
    MODE_FILL,
    MODE_MAX,
};

int main(int argc, char *argv[]) {
    const char* filename = NULL;
    const char* server_path = DEFAULT_SOCK_PATH;
    int mode = MODE_STRETCH;

    int opt = -1;
    while((opt = getopt(argc, argv, "i:s:m:")) != -1)
    {
        switch(opt)
        {
            case 'i':
                filename = optarg;
                break;
            case 's':
                server_path = optarg;
                break;
            case 'm':
                mode = atoi(optarg);
                break;
            default:
                break;
        }
    }
    if (filename == NULL || mode < 0 || mode >= MODE_MAX)
    {
        fprintf(stderr, "Usage: %s -i <input file> [-s <server path>] [-m <mode>]\n", argv[0]);
        fprintf(stderr, "\tModes:\n");
        fprintf(stderr, "\t\t0: Stretch\n");
        fprintf(stderr, "\t\t1: Fit\n");
        fprintf(stderr, "\t\t2: Fill\n");
        return 1;
    }

    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    const AVCodec *codec = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL, *converted_frame = NULL;
    struct SwsContext *sws_ctx = NULL;
    uint8_t *bgr_buffer = NULL;
    image_t* image = NULL;
    int server_fd = -1;
    int video_stream_idx = -1;
    int ret;

    // Connect to server
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        ret = 1;
        fprintf(stderr, "Could not connect to server\n");
        goto cleanup;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, server_path, sizeof(addr.sun_path) - 1);
    size_t addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
    if (addr.sun_path[0] == '@')
    {
        addr.sun_path[0] = '\0';
    }
    if ((ret = connect(server_fd, (struct sockaddr*)&addr, addr_len)) < 0)
    {
        fprintf(stderr, "Could not connect to server\n");
        goto cleanup;
    }

    // Open input file
    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
        fprintf(stderr, "Could not open file '%s'\n", filename);
        goto cleanup;
    }

    // Find stream information
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto cleanup;
    }

    // Find video stream
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1) {
        ret = 1;
        fprintf(stderr, "Could not find video stream\n");
        goto cleanup;
    }

    // Get codec parameters and find decoder
    AVCodecParameters *codec_params = fmt_ctx->streams[video_stream_idx]->codecpar;
    codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        ret = 1;
        fprintf(stderr, "Unsupported codec\n");
        goto cleanup;
    }

    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        ret = 1;
        fprintf(stderr, "Could not allocate codec context\n");
        goto cleanup;
    }

    // Copy codec parameters to context
    if ((ret = avcodec_parameters_to_context(codec_ctx, codec_params)) < 0) {
        fprintf(stderr, "Could not copy codec parameters\n");
        goto cleanup;
    }

    // Open codec
    if ((ret = avcodec_open2(codec_ctx, codec, NULL)) < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto cleanup;
    }

    // Allocate frame and packet
    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if (!frame || !pkt) {
        ret = 1;
        fprintf(stderr, "Could not allocate frame/packet\n");
        goto cleanup;
    }

    // Read frames
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_idx) {
            if ((ret = avcodec_send_packet(codec_ctx, pkt)) < 0) {
                fprintf(stderr, "Error sending packet\n");
                break;
            }

            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_unref(pkt);
                continue;
            } else if (ret < 0) {
                fprintf(stderr, "Error receiving frame\n");
                break;
            }

            // Frame decoded successfully
            break;
        }
        av_packet_unref(pkt);
    }

    // Check if we got a frame
    if (!frame->data[0]) {
        ret = 1;
        fprintf(stderr, "No frame decoded\n");
        goto cleanup;
    }

    int converted_width, converted_height;

    switch (mode)
    {
    case MODE_STRETCH:
        converted_width = CONST_SCREEN_WIDTH;
        converted_height = CONST_SCREEN_HEIGHT;
        break;
    case MODE_FIT:
        if ((float)frame->width / (float)frame->height > (float)CONST_SCREEN_WIDTH / (float)CONST_SCREEN_HEIGHT)
        {
            converted_width = CONST_SCREEN_WIDTH;
            converted_height = frame->height * CONST_SCREEN_WIDTH / frame->width;
        }
        else
        {
            converted_width = frame->width * CONST_SCREEN_HEIGHT / frame->height;
            converted_height = CONST_SCREEN_HEIGHT;
        }
        break;
    case MODE_FILL:
        if ((float)frame->width / (float)frame->height > (float)CONST_SCREEN_WIDTH / (float)CONST_SCREEN_HEIGHT)
        {
            converted_width = frame->width * CONST_SCREEN_HEIGHT / frame->height;
            converted_height = CONST_SCREEN_HEIGHT;
        }
        else
        {
            converted_width = CONST_SCREEN_WIDTH;
            converted_height = frame->height * CONST_SCREEN_WIDTH / frame->width;
        }
        break;
    default:
        ret = 1;
        fprintf(stderr, "Invalid mode\n");
        goto cleanup;
    }

    // Initialize conversion context
    sws_ctx = sws_getContext(frame->width, frame->height, codec_ctx->pix_fmt,
                            converted_width, converted_height, AV_PIX_FMT_BGR24,
                            SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        fprintf(stderr, "Could not create sws context\n");
        goto cleanup;
    }

    // Allocate image
    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, converted_width, converted_height, 1);
    bgr_buffer = av_malloc(buffer_size);
    if (!bgr_buffer) {
        fprintf(stderr, "Could not allocate BGR buffer\n");
        goto cleanup;
    }

    // Allocate converted frame
    converted_frame = av_frame_alloc();
    converted_frame->width = converted_width;
    converted_frame->height = converted_height;
    converted_frame->format = AV_PIX_FMT_BGR24;
    av_image_fill_arrays(converted_frame->data, converted_frame->linesize,
                        bgr_buffer, AV_PIX_FMT_BGR24,
                        converted_frame->width, converted_frame->height, 32);

    // Perform conversion
    sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize,
             0, frame->height, converted_frame->data, converted_frame->linesize);

    // Now you can access the BGR data through converted_frame->data[0]
    // Or copy to a tightly packed buffer:
    image = image_new(CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT);
    if (!image)
    {
        ret = 1;
        fprintf(stderr, "Could not allocate image");
        goto cleanup;
    }
    /** erase the image to black */
    memset(image->pixels, 0, CONST_FB_SIZE);

    int x_offset = (converted_frame->width - CONST_SCREEN_WIDTH) / 2;
    int y_offset = (converted_frame->height - CONST_SCREEN_HEIGHT) / 2;   

    for (int y = 0; y < converted_frame->height; y++)
    {
        if (y < y_offset)
        {
            continue;
        }
        if (y >= y_offset + (int)image->height)
        {
            break;
        }
        for (int x = 0; x < converted_frame->width; x++)
        {
            if (x < x_offset)
            {
                continue;
            }
            if (x >= x_offset + (int)image->width)
            {
                break;
            }
            int src_index = y * converted_frame->linesize[0] + x * 3;
            int dst_index = (y - y_offset) * image->width + (x - x_offset);
            image->pixels[dst_index].bgr.b = converted_frame->data[0][src_index];
            image->pixels[dst_index].bgr.g = converted_frame->data[0][src_index + 1];
            image->pixels[dst_index].bgr.r = converted_frame->data[0][src_index + 2];
        }
    }

    // Send image to server
    if (send(server_fd, image->pixels, CONST_FB_SIZE, SOCK_NONBLOCK) < 0)
    {
        fprintf(stderr, "Could not send image to server\n");
        ret = 1;
        goto cleanup;
    }

    printf("Original dimensions: %dx%d\n", frame->width, frame->height);
    printf("Converted dimensions: %dx%d\n", converted_width, converted_height);

    ret = 0;

cleanup:
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&converted_frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    sws_freeContext(sws_ctx);
    av_free(bgr_buffer);
    image_free(image);
    if (server_fd >= 0)
        close(server_fd);

    return ret;
}
