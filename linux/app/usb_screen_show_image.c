#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "usb_screen_client.h"

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
    const char* filename = NULL;
    const char* server_path = NULL;
    int mode = USB_SCREEN_MODE_STRETCH;

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
    if (filename == NULL || mode < 0 || mode >= USB_SCREEN_MODE_MAX)
    {
        fprintf(stderr, "Usage: %s -i <input file> [-s <server path>] [-m <mode>]\n", argv[0]);
        fprintf(stderr, "\tModes:\n");
        fprintf(stderr, "\t\t0: Stretch\n");
        fprintf(stderr, "\t\t1: Fit\n");
        fprintf(stderr, "\t\t2: Fill\n");
        return 1;
    }

    AVFormatContext* fmt_ctx = NULL;
    AVCodecContext* codec_ctx = NULL;
    const AVCodec* codec = NULL;
    AVPacket* packet = NULL;
    AVFrame* frame = NULL;
    AVStream* stream = NULL;
    int ret;
    usb_screen_client_t* client = NULL;

    // Open input file
    ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL);
    CHECK_EXPR(ret >= 0, "Failed to open input file");

    // Find stream information
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    CHECK_EXPR(ret >= 0, "Failed to find stream info");

    // Find video stream
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            stream = fmt_ctx->streams[i];
            break;
        }
    }
    CHECK_EXPR(stream, "Failed to find video stream");

    // Get codec parameters and find decoder
    AVCodecParameters *codec_params = stream->codecpar;
    codec = avcodec_find_decoder(codec_params->codec_id);
    CHECK_EXPR(codec, "Failed to find decoder");

    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    CHECK_EXPR(codec_ctx, "Failed to allocate codec context");

    // Copy codec parameters to context
    ret = avcodec_parameters_to_context(codec_ctx, codec_params);
    CHECK_EXPR(ret >= 0, "Failed to copy codec parameters");

    // Open codec
    ret = avcodec_open2(codec_ctx, codec, NULL);
    CHECK_EXPR(ret >= 0, "Failed to open codec");

    // Allocate frame and packet
    frame = av_frame_alloc();
    CHECK_EXPR(frame, "Failed to allocate frame");
    packet = av_packet_alloc();
    CHECK_EXPR(packet, "Failed to allocate packet");

    // Read frames
    while (av_read_frame(fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index != stream->index)
        {
            av_packet_unref(packet);
            continue;
        }
        ret = avcodec_send_packet(codec_ctx, packet);
        CHECK_EXPR(ret >= 0, "Failed to send packet");
        av_packet_unref(packet);
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            continue;
        }
        CHECK_EXPR(ret >= 0, "Failed to receive frame");
    }
    CHECK_EXPR(frame->data[0], "No frame decoded");

    // Connect to server and send frame
    usb_screen_client_option_t client_option;
    memset(&client_option, 0, sizeof(client_option));
    client_option.server_path = server_path;
    client_option.frame_format = stream->codecpar->format;
    client_option.frame_width = stream->codecpar->width;
    client_option.frame_height = stream->codecpar->height;
    client_option.mode = mode;
    client = usb_screen_client_connect(&client_option);
    CHECK_EXPR(client, "Failed to connect to server");
    ret = client->send_frame(client, frame);
    CHECK_EXPR(ret == 0, "Failed to send frame");

    // Clean up.
    client->close(client);
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    return 0;
}
