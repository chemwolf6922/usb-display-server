#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "image.h"

typedef struct
{
    pixel_t* color_palettes;
    int k;
    uint32_t* pixel_indexs;
    size_t width;
    size_t height;
} color_palette_image_t;

int k_means_compression(const image_t* src, int k, color_palette_image_t* dst);
int paint_color_palette_image(const color_palette_image_t* src, image_t* dst);

color_palette_image_t* color_palette_image_new(int k, int width, int height);
void color_palette_image_free(color_palette_image_t* image);

#ifdef __cplusplus
}
#endif
