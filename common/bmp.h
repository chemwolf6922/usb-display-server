#pragma once

#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    char signature[2];
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} __attribute__((packed)) bmp_header_t;

typedef struct
{
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t size_image;
    int32_t x_pels_per_meter;
    int32_t y_pels_per_meter;
    uint32_t clr_used;
    uint32_t clr_important;
} __attribute__((packed)) dib_header_t;

#define BMP_24BIT_ROW_SIZE(width) (((width) * 3 + 3) & ~3)
#define BMP_24BIT_SIZE(width, height) (sizeof(bmp_header_t) + sizeof(dib_header_t) + BMP_24BIT_ROW_SIZE(width) * (height))

image_t* load_24bit_bmp(const char* filename);
int load_24bit_bmp_from_ram(const uint8_t* data, size_t size, image_t* image);
void dump_image_to_bmp(const char* filename, const image_t* image);

#ifdef __cplusplus
}
#endif
