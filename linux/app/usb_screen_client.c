#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include "../server/config.h"
#include "../../common/image.h"

#include "usb_screen_client.h"


typedef struct
{
    usb_screen_client_t base;
    int fd;
    int frame_width;
    int frame_height;
    int mode;
    AVFrame* resized_frame;
    struct SwsContext* sws_context;
    image_t* image;
} usb_screen_client_impl_t;

static void usb_screen_client_close(usb_screen_client_t* base);
static int get_resized_frame_dimensions(
    int src_width, int src_height,
    int dst_width, int dst_height,
    int mode,
    int* resized_width, int* resized_height);
static int usb_screen_client_send_frame(usb_screen_client_t* self, const AVFrame* frame);

usb_screen_client_t* usb_screen_client_connect(const usb_screen_client_option_t* option)
{

#define CHECK_EXPR(expr, message) \
do \
{ \
    if (!(expr)) \
    { \
        fprintf(stderr, "[%s] %s: %s\n", __func__, #expr, message); \
        goto error; \
    } \
} while (0)

    usb_screen_client_impl_t* this = malloc(sizeof(usb_screen_client_impl_t));
    CHECK_EXPR(this, "Failed to allocate memory");
        
    memset(this, 0, sizeof(usb_screen_client_impl_t));
    this->fd = -1;
    this->base.close = usb_screen_client_close;
    this->base.send_frame = usb_screen_client_send_frame;

    int resized_width, resized_height;
    int rc = get_resized_frame_dimensions(
        option->frame_width, option->frame_height,
        CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT,
        option->mode,
        &resized_width, &resized_height);
    CHECK_EXPR(rc == 0, "Failed to get resized frame dimensions");

    this->resized_frame = av_frame_alloc();
    CHECK_EXPR(this->resized_frame, "Failed to allocate resized frame");
    
    this->resized_frame->format = AV_PIX_FMT_RGB24;
    this->resized_frame->width = resized_width;
    this->resized_frame->height = resized_height;
    rc = av_image_alloc(
        this->resized_frame->data, this->resized_frame->linesize,
        this->resized_frame->width, this->resized_frame->height,
        this->resized_frame->format, 32);
    CHECK_EXPR(rc >= 0, "Failed to allocate resized frame data");
    
    this->sws_context = sws_getContext(
        option->frame_width, option->frame_height, option->frame_format,
        this->resized_frame->width, this->resized_frame->height, this->resized_frame->format,
        SWS_BICUBIC, NULL, NULL, NULL);
    CHECK_EXPR(this->sws_context, "Failed to create sws context");

    this->image = image_new(CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT);
    CHECK_EXPR(this->image, "Failed to allocate image");

    this->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK_EXPR(this->fd >= 0, "Failed to create socket");
    const char* server_path = option->server_path ? option->server_path : DEFAULT_SOCK_PATH;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, server_path, sizeof(addr.sun_path) - 1);
    size_t addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
    if (addr.sun_path[0] == '@')
        addr.sun_path[0] = '\0';
    rc = connect(this->fd, (struct sockaddr*)&addr, addr_len);
    CHECK_EXPR(rc >= 0, "Failed to connect to server");   

    return &this->base;
error:
    usb_screen_client_close((usb_screen_client_t*)this);
    return NULL;

#undef CHECK_EXPR

}

static void usb_screen_client_close(usb_screen_client_t* base)
{
    usb_screen_client_impl_t* this = (usb_screen_client_impl_t*)base;
    if (!this)
        return;
    if (this->fd >= 0)
        close(this->fd);
    if (this->image)
        image_free(this->image);
    if (this->sws_context)
        sws_freeContext(this->sws_context);
    if (this->resized_frame)
    {
        av_freep(&this->resized_frame->data[0]);
        av_frame_free(&this->resized_frame);
    }
    free(this);
}

static int get_resized_frame_dimensions(
    int src_width, int src_height,
    int dst_width, int dst_height,
    int mode,
    int* resized_width, int* resized_height)
{
    switch (mode)
    {
    case USB_SCREEN_MODE_STRETCH:
        *resized_width = dst_width;
        *resized_height = dst_height;
        break;
    case USB_SCREEN_MODE_FIT:
        if ((float)src_width / (float)src_height > (float)dst_width / (float)dst_height)
        {
            *resized_width = dst_width;
            *resized_height = src_height * dst_width / src_width;
        }
        else
        {
            *resized_width = src_width * dst_height / src_height;
            *resized_height = dst_height;
        }
        break;
    case USB_SCREEN_MODE_FILL:
        if ((float)src_width / (float)src_height > (float)dst_width / (float)dst_height)
        {
            *resized_width = src_width * dst_height / src_height;
            *resized_height = dst_height;
        }
        else
        {
            *resized_width = dst_width;
            *resized_height = src_height * dst_width / src_width;
        }
        break;
    default:
        return -1;
    }
    return 0;
}

static int usb_screen_client_send_frame(usb_screen_client_t* self, const AVFrame* frame)
{
    usb_screen_client_impl_t* this = (usb_screen_client_impl_t*)self;
    if (!this || !frame)
        return -1;
    
    int rc = sws_scale(
        this->sws_context,
        (const uint8_t* const*)frame->data,
        frame->linesize,
        0,
        frame->height,
        this->resized_frame->data,
        this->resized_frame->linesize);
    if (rc < 0)
        return -1;

    memset(this->image->pixels, 0, this->image->width * this->image->height * sizeof(pixel_t));
    int x_offset = (this->resized_frame->width - this->image->width) / 2;
    int y_offset = (this->resized_frame->height - this->image->height) / 2;
    for (int y = 0; y < this->resized_frame->height; y++)
    {
        if (y < y_offset)
            continue;
        if (y >= y_offset + (int)this->image->height)
            break;
        for (int x = 0; x < this->resized_frame->width; x++)
        {
            if (x < x_offset)
                continue;
            if (x >= x_offset + (int)this->image->width)
                break;
            int src_index = y * this->resized_frame->linesize[0] + x * 3;
            int dst_index = (y - y_offset) * this->image->width + (x - x_offset);
            this->image->pixels[dst_index].bgr.r = this->resized_frame->data[0][src_index];
            this->image->pixels[dst_index].bgr.g = this->resized_frame->data[0][src_index + 1];
            this->image->pixels[dst_index].bgr.b = this->resized_frame->data[0][src_index + 2];
        }
    }

    if (send(this->fd, this->image->pixels, CONST_FB_SIZE, SOCK_NONBLOCK) < 0)
        return -1;

    return 0;
}
