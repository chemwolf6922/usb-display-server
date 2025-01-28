/**
 * @brief The server accepts bmp frames on a pipe, 
 *     and sends the compressed frames to the device. 
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "usb_screen.h"
#include "config.h"
#include "tev/tev.h"
#include "../../common/k_means_compression.h"
#include "../../common/bmp.h"
#include "../../common/color_conversion.h"

typedef struct
{
    int pipe_fd;
    int fake_write_end_fd;
    usb_screen_t* screen;
    tev_handle_t* tev;
    tev_timeout_handle_t frame_sync;
    uint8_t buffer_a[INPUT_BMP_SIZE];
    uint8_t buffer_b[INPUT_BMP_SIZE];
    uint8_t* buffer;
    int read_len;
    uint64_t last_frame_time_ms;
    image_t* image;
    color_palette_image_t* compressed_image;
    packed_color_palette_image_t* packed_image;
    bool first_frame;
} app_t;

static app_t app;

static void on_pipe_data(void* );
static void process_frame(void* );
static uint64_t now_ms();

int main(int argc, char* const* argv)
{
    /** parse args */
    const char* device = NULL;
    const char* pipe_path = DEFAULT_PIPE_PATH;

    int opt = -1;
    while ((opt = getopt(argc, argv, "p:d:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            pipe_path = optarg;
            break;
        case 'd':
            device = optarg;
            break;
        default:
            break;
        }
    }
    if (device == NULL)
    {
        fprintf(stderr, "Usage: %s -d <device> [-p <pipe path>]\n", argv[0]);
        return 1;
    }

    memset(&app, 0, sizeof(app_t));
    app.first_frame = true;
    app.buffer = app.buffer_a;
    app.image = image_new(CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT);
    if (!app.image)
    {
        fprintf(stderr, "Failed to create image\n");
        return 1;
    }
    app.compressed_image = color_palette_image_new(CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT, CONST_N_COLOR);
    if (!app.compressed_image)
    {
        fprintf(stderr, "Failed to create compressed image\n");
        return 1;
    }
    app.packed_image = packed_color_palette_image_new(CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT, CONST_N_COLOR);
    if (!app.packed_image)
    {
        fprintf(stderr, "Failed to create packed image\n");
        return 1;
    }
    
    /** Create the named pipe */
    if (mkfifo(pipe_path, 0666) == -1)
    {
        perror("mkfifo");
        return 1;
    }

    /** Open the named pipe */
    app.pipe_fd = open(pipe_path, O_RDONLY);
    if (app.pipe_fd == -1)
    {
        perror("open");
        return 1;
    }

    /** Open a fake write end to keep the pipe open */
    app.fake_write_end_fd = open(pipe_path, O_WRONLY);
    if (app.fake_write_end_fd == -1)
    {
        perror("open");
        return 1;
    }

    /** Open the device */
    app.screen = usb_screen_open(device);
    if (app.screen == NULL)
    {
        fprintf(stderr, "Failed to open the device\n");
        return 1;
    }

    /** Event loop */
    app.tev = tev_create_ctx();
    if (app.tev == NULL)
    {
        fprintf(stderr, "Failed to create the event loop\n");
        return 1;
    }

    tev_set_read_handler(app.tev, app.pipe_fd, on_pipe_data, NULL);

    tev_main_loop(app.tev);

    tev_free_ctx(app.tev);

    close(app.fake_write_end_fd);
    close(app.pipe_fd);
    app.screen->close(app.screen);
    image_free(app.image);
    color_palette_image_free(app.compressed_image);
    packed_color_palette_image_free(app.packed_image);

    /* code */
    return 0;
}

static void app_exit()
{
    tev_clear_timeout(app.tev, app.frame_sync);
    tev_set_read_handler(app.tev, app.pipe_fd, NULL, NULL);
}

static void on_pipe_data(void* )
{
    int read_len = read(app.pipe_fd, app.buffer + app.read_len, sizeof(app.buffer) - app.read_len);
    if (read_len == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        perror("read");
        /** This should quit the event loop */
        app_exit();
        return;
    }
    if (read_len == 0)
    {
        /** EOF */
        app_exit();
        return;
    }
    app.read_len += read_len;
    if (app.read_len == sizeof(app.buffer))
    {
        /** Frame is ready */
        app.read_len = 0;
        app.buffer = app.buffer == app.buffer_a ? app.buffer_b : app.buffer_a;
        uint64_t now = now_ms();
        if (now - app.last_frame_time_ms < DEFAULT_FRAME_MIN_INTERVAL && app.frame_sync == NULL)
        {
            /** Wait for the next frame */
            app.frame_sync = tev_set_timeout(
                app.tev, 
                process_frame, NULL, 
                DEFAULT_FRAME_MIN_INTERVAL - (now - app.last_frame_time_ms));
            return;
        }
        else if(app.frame_sync == NULL)
        {
            app.last_frame_time_ms = now;
            process_frame(NULL);
        }
        /** else: There is a pending timer. Do nothing */
    }
}

static void process_frame(void* )
{
    if (app.frame_sync != NULL)
    {
        /** This is redundant */
        tev_clear_timeout(app.tev, app.frame_sync);
        app.frame_sync = NULL;
    }
    uint8_t* ready_buffer = app.buffer == app.buffer_a ? app.buffer_b : app.buffer_a;
    /** compress image, this can be time consuming */
    if (load_24bit_bmp_from_ram(ready_buffer, INPUT_BMP_SIZE, app.image) != 0)
    {
        fprintf(stderr, "Failed to load BMP from RAM\n");
        return;
    }
    bgr_image_to_ycbcr(app.image, app.image);
    if (k_means_compression(app.image, CONST_N_COLOR, app.compressed_image, !app.first_frame) != 0)
    {
        fprintf(stderr, "Failed to compress image\n");
        return;
    }
    pixel_t color_palette[CONST_N_COLOR];
    memcpy(color_palette, app.compressed_image->color_palettes, sizeof(color_palette));
    palette_ycbcr_to_bgr(app.compressed_image, app.compressed_image);
    pack_color_palette_image(app.compressed_image, app.packed_image);
    /** DO not care if this fails */
    app.screen->write(app.screen, app.packed_image->data, app.packed_image->size);
    memcpy(app.compressed_image->color_palettes, color_palette, sizeof(color_palette));
    app.first_frame = false;
}

static uint64_t now_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

