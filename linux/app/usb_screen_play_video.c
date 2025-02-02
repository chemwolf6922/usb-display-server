#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "usb_screen_client.h"

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
    int rc = 0;
    const char* input_file = NULL;
    const char* server_path = NULL;
    int mode = USB_SCREEN_MODE_STRETCH;

    int opt = -1;
    while ((opt = getopt(argc, argv, "s:i:m:")) != -1)
    {
        switch (opt)
        {
        case 's':
            server_path = optarg;
            break;
        case 'i':
            input_file = optarg;
            break;
        case 'm':
            mode = atoi(optarg);
            break;
        default:
            break;
        }
    }
    if (input_file == NULL || mode < 0 || mode >= USB_SCREEN_MODE_MAX)
    {
        fprintf(stderr, "Usage: %s -i <input file> [-s <server path>] [-m <mode>]\n", argv[0]);
        fprintf(stderr, "\tModes:\n");
        fprintf(stderr, "\t\t0: Stretch\n");
        fprintf(stderr, "\t\t1: Fit\n");
        fprintf(stderr, "\t\t2: Fill\n");
        return 1;
    }

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

    usb_screen_client_option_t client_option;
    memset(&client_option, 0, sizeof(client_option));
    client_option.server_path = server_path;
    client_option.frame_width = input_stream->codecpar->width;
    client_option.frame_height = input_stream->codecpar->height;
    client_option.frame_format = input_stream->codecpar->format;
    client_option.mode = mode;
    usb_screen_client_t* client = usb_screen_client_connect(&client_option);
    CHECK_EXPR(client, "Failed to connect to server");

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
                rc = client->send_frame(client, frame);
                CHECK_EXPR(rc == 0, "Failed to send frame");
                n_frame++;
                av_frame_unref(frame);

                /** @todo improve frame pacing */
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
    
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&decoder_context);
    avformat_close_input(&format_context);
    client->close(client);
    return 0;
}

static uint64_t now_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

