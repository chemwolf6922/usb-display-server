#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavutil/avutil.h>

#include "usb_screen_client.h"

#define CHECK_EXPR(expr, message) \
do { \
    if (!(expr)) \
    { \
        fprintf(stderr, "[%d] %s: %s\n", __LINE__, #expr, message); \
        return 1; \
    } \
} while(0)

#define DEFAULT_LISTEN_PORT 61089

int main(int argc, char* const* argv)
{
    /** parse options */
    int opt = -1;
    const char* server_path = NULL;
    int mode = USB_SCREEN_MODE_STRETCH;
    int port = DEFAULT_LISTEN_PORT;
    while ((opt = getopt(argc, argv, "s:m:l:")) != -1)
    {
        switch (opt)
        {
        case 's':
            server_path = optarg;
            break;
        case 'm':
            mode = atoi(optarg);
            break;
        case 'l':
            port = atoi(optarg);
            break;
        default:
            break;
        }
    }
    if (mode < 0 || mode >= USB_SCREEN_MODE_MAX || port < 0 || port >= 65536)
    {
        fprintf(stderr, "Invalid arguments\n");
        fprintf(stderr, "Usage: %s [-s <server path>] [-m <mode>] [-l <listen port>]\n", argv[0]);
        fprintf(stderr, "\tModes:\n");
        fprintf(stderr, "\t\t0: Stretch\n");
        fprintf(stderr, "\t\t1: Fit\n");
        fprintf(stderr, "\t\t2: Fill\n");
        return 1;
    }

    /** Open rtmp server */
    AVFormatContext* fmt_ctx = NULL;
    AVDictionary* options = NULL;
    char url[100] = {0};
    snprintf(url, sizeof(url), "rtmp://localhost:%d/live/stream", port);
    av_dict_set(&options, "listen", "1", 0);

    printf("Open rtmp server: %s\n", url);

    int rc = avformat_open_input(&fmt_ctx, url, NULL, &options);
    CHECK_EXPR(rc >= 0, "Failed to open input");
    av_dict_free(&options);

    /** Find the video stream */
    rc = avformat_find_stream_info(fmt_ctx, NULL);
    CHECK_EXPR(rc >= 0, "Failed to find stream info");
    AVStream* stream = NULL;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            stream = fmt_ctx->streams[i];
            break;
        }
    }
    CHECK_EXPR(stream, "Failed to find video stream");
    CHECK_EXPR(stream->codecpar->codec_id == AV_CODEC_ID_H264, "Only H264 is supported");

    /** Connect to server */
    usb_screen_client_option_t client_option;
    memset(&client_option, 0, sizeof(client_option));
    client_option.server_path = server_path;
    client_option.frame_format = stream->codecpar->format;
    client_option.frame_width = stream->codecpar->width;
    client_option.frame_height = stream->codecpar->height;
    client_option.mode = mode;
    usb_screen_client_t* client = usb_screen_client_connect(&client_option);
    CHECK_EXPR(client, "Failed to connect to server");

    /** Create decoder */
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    CHECK_EXPR(codec, "Failed to find decoder");
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    CHECK_EXPR(codec_ctx, "Failed to allocate codec context");
    avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    avcodec_open2(codec_ctx, codec, NULL);

    /** Create filter */
    AVBSFContext* bsf_ctx = NULL;
    const AVBitStreamFilter *filter = av_bsf_get_by_name("h264_mp4toannexb");
    rc = av_bsf_alloc(filter, &bsf_ctx);
    CHECK_EXPR(rc == 0, "Failed to allocate bsf context");
    avcodec_parameters_copy(bsf_ctx->par_in, stream->codecpar);
    av_bsf_init(bsf_ctx);

    /** Receive and decode frames */
    AVPacket* packet = av_packet_alloc();
    CHECK_EXPR(packet, "Failed to allocate packet");
    AVFrame* frame = av_frame_alloc();
    CHECK_EXPR(frame, "Failed to allocate frame");
    while (av_read_frame(fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index != stream->index)
        {
            av_packet_unref(packet);
            continue;
        }

        av_bsf_send_packet(bsf_ctx, packet);
        av_packet_unref(packet);

        while((rc = av_bsf_receive_packet(bsf_ctx, packet)) >= 0)
        {
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
            {
                break;
            }
            CHECK_EXPR(rc == 0, "Failed to receive packet");

            avcodec_send_packet(codec_ctx, packet);
            av_packet_unref(packet);

            while((rc = avcodec_receive_frame(codec_ctx, frame)) >= 0)
            {
                if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
                {
                    break;
                }
                CHECK_EXPR(rc == 0, "Failed to receive frame");
                
                rc = client->send_frame(client, frame);
                CHECK_EXPR(rc == 0, "Failed to send frame");

                av_frame_unref(frame);
            }
        }
    }
    av_packet_free(&packet);
    
    avcodec_close(codec_ctx);
    avformat_close_input(&fmt_ctx);

    return 0;
}

