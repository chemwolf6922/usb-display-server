#pragma once

#include <libavutil/frame.h>

enum
{
    USB_SCREEN_MODE_STRETCH,
    USB_SCREEN_MODE_FIT,
    USB_SCREEN_MODE_FILL,
    USB_SCREEN_MODE_MAX,
};

typedef struct
{
    const char* server_path;
    int frame_width;
    int frame_height;
    enum AVPixelFormat frame_format;
    int mode;
} usb_screen_client_option_t;

typedef struct usb_screen_client_s usb_screen_client_t;
struct usb_screen_client_s
{
    void (*close)(usb_screen_client_t* self);
    int (*send_frame)(usb_screen_client_t* self, const AVFrame* frame);
};

usb_screen_client_t* usb_screen_client_connect(const usb_screen_client_option_t* option);
