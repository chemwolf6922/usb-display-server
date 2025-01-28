#pragma once

#include <stddef.h>

typedef struct usb_screen_s usb_screen_t;

struct usb_screen_s
{
    void(*close)(usb_screen_t* self);
    int(*write)(usb_screen_t* self, const void* data, size_t size);
};

usb_screen_t* usb_screen_open(const char* device);
