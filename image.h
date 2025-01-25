#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

enum
{
    COLOR_SPACE_bgr,
    COLOR_SPACE_YCBCR,
};

typedef struct
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
} bgr_pixel_t;

typedef struct
{
    uint8_t y;
    int8_t cb;
    int8_t cr;
} ycbcr_pixel_t;

typedef union
{
    bgr_pixel_t bgr;
    ycbcr_pixel_t ycbcr;
} pixel_t;

typedef struct
{
    pixel_t* pixels;
    size_t width;
    size_t height;
    int color_space;
} image_t;

#ifdef __cplusplus
}
#endif