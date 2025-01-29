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
#include <sys/socket.h>
#include <sys/un.h>

#include "usb_screen.h"
#include "config.h"
#include "tev/tev.h"
#include "tev/map.h"
#include "../../common/k_means_compression.h"
#include "../../common/image.h"
#include "../../common/color_conversion.h"

typedef struct
{
    int fd;
    uint8_t buffer[CONST_FB_SIZE];
    size_t read_len;
} client_t;

typedef struct
{
    int fd;
    map_handle_t clients;
    usb_screen_t* screen;
    tev_handle_t* tev;
    tev_timeout_handle_t frame_sync;
    uint64_t last_frame_time_ms;
    image_t* image;
    color_palette_image_t* compressed_image;
    packed_color_palette_image_t* packed_image;
    bool first_frame;
} app_t;

static app_t app;

static void on_client_connection(void* );
static void on_client_data(void* ctx);
static void process_frame(void* );
static uint64_t now_ms();
static client_t* client_new(int fd);
static void client_free(void* data, void* );

int main(int argc, char* const* argv)
{
    /** parse args */
    const char* device = NULL;
    const char* sock_path = DEFAULT_SOCK_PATH;

    int opt = -1;
    while ((opt = getopt(argc, argv, "l:d:")) != -1)
    {
        switch (opt)
        {
        case 'l':
            sock_path = optarg;
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
        fprintf(stderr, "Usage: %s -d <device> [-l <listen path>]\n", argv[0]);
        return 1;
    }

    memset(&app, 0, sizeof(app_t));
    app.first_frame = true;
    app.clients = map_create();
    if (!app.clients)
    {
        fprintf(stderr, "Failed to create map\n");
        return 1;
    }
    app.image = image_new(CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT);
    if (!app.image)
    {
        fprintf(stderr, "Failed to create image\n");
        return 1;
    }
    app.compressed_image = color_palette_image_new(CONST_N_COLOR, CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT);
    if (!app.compressed_image)
    {
        fprintf(stderr, "Failed to create compressed image\n");
        return 1;
    }
    app.packed_image = packed_color_palette_image_new(CONST_N_COLOR, CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT);
    if (!app.packed_image)
    {
        fprintf(stderr, "Failed to create packed image\n");
        return 1;
    }

    app.fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (app.fd == -1)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    size_t addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
    if (addr.sun_path[0] == '@')
    {
        addr.sun_path[0] = '\0';
    }
    if (bind(app.fd, (struct sockaddr*)&addr, addr_len) != 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(app.fd, 5) != 0)
    {
        perror("listen");
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

    tev_set_read_handler(app.tev, app.fd, on_client_connection, NULL);

    tev_main_loop(app.tev);

    tev_free_ctx(app.tev);

    close(app.fd);
    app.screen->close(app.screen);
    image_free(app.image);
    color_palette_image_free(app.compressed_image);
    packed_color_palette_image_free(app.packed_image);
    map_delete(app.clients, NULL, NULL);

    /* code */
    return 0;
}

static void app_exit()
{
    tev_clear_timeout(app.tev, app.frame_sync);
    tev_set_read_handler(app.tev, app.fd, NULL, NULL);
    map_entry_t entry;
    map_forEach(app.clients, entry)
    {
        client_t* client = (client_t*)entry.value;
        tev_set_read_handler(app.tev, client->fd, NULL, NULL);
        close(client->fd);
    }
    map_clear(app.clients, client_free, NULL);
}

static void on_client_connection(void* )
{
    int client_fd = accept(app.fd, NULL, NULL);
    if (client_fd == -1)
    {
        perror("accept");
        app_exit();
        return;
    }
    client_t* client = client_new(client_fd);
    if (client == NULL)
    {
        close(client_fd);
        return;
    }
    tev_set_read_handler(app.tev, client_fd, on_client_data, client);
    map_add(app.clients, &client_fd, sizeof(client_fd), client);
}

static void on_client_data(void* ctx)
{
    client_t* client = (client_t*)ctx;
    int read_len = (int)recv(client->fd, client->buffer + client->read_len, sizeof(client->buffer) - client->read_len, SOCK_NONBLOCK);
    if (read_len == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        tev_set_read_handler(app.tev, client->fd, NULL, NULL);
        close(client->fd);
        map_remove(app.clients, &client->fd, sizeof(client->fd));
        client_free(client, NULL);
        return;
    }
    if (read_len == 0)
    {
        /** EOF */
        tev_set_read_handler(app.tev, client->fd, NULL, NULL);
        close(client->fd);
        map_remove(app.clients, &client->fd, sizeof(client->fd));
        client_free(client, NULL);
        return;
    }
    client->read_len += read_len;
    if (client->read_len == sizeof(client->buffer))
    {
        /** Frame is ready */
        memcpy(app.image->pixels, client->buffer, sizeof(client->buffer));
        client->read_len = 0;
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

    app.last_frame_time_ms = now_ms();
    bgr_image_to_ycbcr(app.image, app.image);
    /** compress image, this can be time consuming */
    if (k_means_compression(app.image, CONST_N_COLOR, app.compressed_image, !app.first_frame) < 0)
    {
        fprintf(stderr, "Failed to compress image\n");
        return;
    }
    pixel_t color_palette[CONST_N_COLOR];
    memcpy(color_palette, app.compressed_image->color_palettes, sizeof(color_palette));
    palette_ycbcr_to_bgr(app.compressed_image, app.compressed_image);
    pack_color_palette_image(app.compressed_image, app.packed_image);
    memcpy(app.compressed_image->color_palettes, color_palette, sizeof(color_palette));
    /** DO not care if this fails */
    app.screen->write(app.screen, app.packed_image->data, app.packed_image->size);
    app.first_frame = false;
}

static uint64_t now_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static client_t* client_new(int fd)
{
    client_t* client = (client_t*)malloc(sizeof(client_t));
    if (client == NULL)
    {
        return NULL;
    }
    client->read_len = 0;
    client->fd = fd;
    return client;
}

static void client_free(void* data, void* )
{
    client_t* client = (client_t*)data;
    free(client);
}
