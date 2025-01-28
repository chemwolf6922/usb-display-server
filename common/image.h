#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

enum
{
    COLOR_SPACE_BGR,
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

typedef struct
{
    pixel_t* color_palettes;
    int k;
    int color_space;
    uint32_t* pixel_indexs;
    size_t width;
    size_t height;
} color_palette_image_t;

typedef struct
{
    uint8_t* data;
    size_t size;
} packed_color_palette_image_t;

image_t* image_new(size_t width, size_t height);
void image_free(image_t* image);

color_palette_image_t* color_palette_image_new(int k, int width, int height);
void color_palette_image_free(color_palette_image_t* image);

int paint_color_palette_image(const color_palette_image_t* src, image_t* dst);

packed_color_palette_image_t* packed_color_palette_image_new(int k, int width, int height);
void packed_color_palette_image_free(packed_color_palette_image_t* image);

int pack_color_palette_image(const color_palette_image_t* src, packed_color_palette_image_t* dst);

#ifdef __cplusplus
}
#endif