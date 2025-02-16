#include "usb_screen.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

typedef struct
{
    usb_screen_t base;
    int fd;
    char* device_path;
} usb_screen_impl_t;

static void usb_screen_close(usb_screen_t* base);
static int usb_screen_write(usb_screen_t* base, const void* data, size_t size);
static void try_open_device(usb_screen_impl_t* this);

usb_screen_t* usb_screen_open(const char* device)
{
    usb_screen_impl_t* screen = malloc(sizeof(usb_screen_impl_t));
    if (screen == NULL)
    {
        return NULL;
    }
    memset(screen, 0, sizeof(usb_screen_impl_t));
    screen->base.close = usb_screen_close;
    screen->base.write = usb_screen_write;
    screen->fd = -1;
    screen->device_path = strdup(device);
    if (screen->device_path == NULL)
    {
        free(screen);
        return NULL;
    }
    /** Try to open the device */
    try_open_device(screen);

    return &screen->base;
}

static void usb_screen_close(usb_screen_t* base)
{
    if (!base)
    {
        return;
    }
    usb_screen_impl_t* this = (usb_screen_impl_t*)base;
    if (this->fd != -1)
    {
        close(this->fd);
        this->fd = -1;
    }
    if (this->device_path)
    {
        free(this->device_path);
        this->device_path = NULL;
    }
    free(this);
}

static int usb_screen_write(usb_screen_t* base, const void* data, size_t size)
{
    if (!base || !data || size == 0)
    {
        return -1;
    }
    usb_screen_impl_t* this = (usb_screen_impl_t*)base;
    if (this->fd == -1)
    {
        try_open_device(this);
    }
    if (this->fd == -1)
    {
        return -1;
    }
    /**
     * @todo Use non-blocking write.
     * Need to handle incomplete writes with non-blocking write.
     */
    ssize_t write_len = write(this->fd, data, size);
    if (write_len == -1)
    {
        close(this->fd);
        this->fd = -1;
        return -1;
    }
    return 0;
}

static void try_open_device(usb_screen_impl_t* this)
{
    int flags = O_RDWR | O_NOCTTY;
    this->fd = open(this->device_path, flags);
    if (this->fd < 0)
    {
        return;
    }

    struct termios tty;
    if (tcgetattr(this->fd, &tty) != 0)
    {
        close(this->fd);
        this->fd = -1;
        return;
    }

    cfmakeraw(&tty);
    tty.c_cflag |= CLOCAL | CREAD;
    tcsetattr(this->fd, TCSANOW, &tty);
}


