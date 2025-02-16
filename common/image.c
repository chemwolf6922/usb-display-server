#include "image.h"
#include <stdlib.h>
#include <string.h>

image_t* image_new(size_t width, size_t height)
{
    image_t* image = (image_t*)malloc(sizeof(image_t));
    if (!image)
    {
        return NULL;
    }
    image->width = width;
    image->height = height;
    image->color_space = COLOR_SPACE_BGR;
    image->pixels = (pixel_t*)malloc(width * height * sizeof(pixel_t));
    if (!image->pixels)
    {
        free(image);
        return NULL;
    }
    return image;
}

void image_free(image_t* image)
{
    if (image)
    {
        if (image->pixels)
        {
            free(image->pixels);
        }
        free(image);
    }
}

color_palette_image_t* color_palette_image_new(int k, int width, int height)
{
    color_palette_image_t* image = malloc(sizeof(color_palette_image_t));
    if (!image)
    {
        return NULL;
    }
    image->k = k;
    image->width = width;
    image->height = height;
    image->color_space = COLOR_SPACE_YCBCR;
    image->color_palettes = malloc(k * sizeof(pixel_t));
    if (!image->color_palettes)
    {
        free(image);
        return NULL;
    }
    image->pixel_indexs = malloc(width * height * sizeof(uint32_t));
    if (!image->pixel_indexs)
    {
        free(image->color_palettes);
        free(image);
        return NULL;
    }
    return image;
}

void color_palette_image_free(color_palette_image_t* image)
{
    if (image)
    {
        if (image->color_palettes)
            free(image->color_palettes);
        if (image->pixel_indexs)
            free(image->pixel_indexs);
        free(image);
    }
}

int paint_color_palette_image(const color_palette_image_t* src, image_t* dst)
{
    if (!src || !dst)
    {
        return -1;
    }
    if (src->height != dst->height || src->width != dst->width)
    {
        return -1;
    }
    for (size_t i = 0; i < dst->width * dst->height; i++)
    {
        int index = src->pixel_indexs[i];
        if (index < 0 || index >= src->k)
        {
            return -1;
        }
        dst->pixels[i].ycbcr = src->color_palettes[index].ycbcr;
    }
    return 0;
}

static size_t get_packed_color_palette_image_size(int k, int width, int height)
{
    int bits_per_pixel = 0;
    for (int i = k - 1; i != 0; i >>= 1)
    {
        bits_per_pixel++;
    }
    return k * 2 + (width * height * bits_per_pixel + 7) / 8;
}

packed_color_palette_image_t* packed_color_palette_image_new(int k, int width, int height)
{
    packed_color_palette_image_t* image = malloc(sizeof(packed_color_palette_image_t));
    if (!image)
    {
        return NULL;
    }
    /** Color palette in BGR565 format, 2 bytes per color */
    image->size = get_packed_color_palette_image_size(k, width, height);
    image->data = malloc(image->size);
    if (!image->data)
    {
        free(image);
        return NULL;
    }
    return image;
}

void packed_color_palette_image_free(packed_color_palette_image_t* image)
{
    if (image)
    {
        if (image->data)
            free(image->data);
        free(image);
    }
}

int pack_color_palette_image(const color_palette_image_t* src, packed_color_palette_image_t* dst)
{
    if (!src || !dst)
    {
        return -1;
    }
    if (dst->size != get_packed_color_palette_image_size(src->k, src->width, src->height))
    {
        return -1;
    }
    memset(dst->data, 0, dst->size);
    for (int i = 0; i < src->k; i++)
    {
        uint16_t rgb565 = ((int)src->color_palettes[i].bgr.b >> 3)
            | (((int)src->color_palettes[i].bgr.g >> 2) << 5)
            | (((int)src->color_palettes[i].bgr.r >> 3) << 11);
        memcpy(dst->data + i * 2, &rgb565, 2);
    }
    uint8_t* pos = dst->data + src->k * 2;
    uint8_t bit_pos = 0;
    int bits_per_pixel = 0;
    for (int k = src->k - 1; k != 0; k >>= 1)
    {
        bits_per_pixel++;
    }
    for (size_t i = 0; i < src->width * src->height; i++)
    {
        int index = src->pixel_indexs[i];
        if (index < 0 || index >= src->k)
        {
            return -1;
        }
        int remaining_bits = bits_per_pixel;
        while (remaining_bits > 0)
        {
            int bits_to_write = remaining_bits < 8 - bit_pos ? remaining_bits : 8 - bit_pos;
            *pos |= index << bit_pos;
            index >>= bits_to_write;
            remaining_bits -= bits_to_write;
            bit_pos += bits_to_write;
            if (bit_pos == 8)
            {
                bit_pos = 0;
                pos++;
            }
        }
    }
    return 0;
}

rgb565_image_t* rgb565_image_new(size_t size)
{
    rgb565_image_t* image = malloc(sizeof(rgb565_image_t));
    if (!image)
    {
        return NULL;
    }
    image->size = size;
    image->pixels = malloc(size * sizeof(rgb565_pixel_t));
    if (!image->pixels)
    {
        free(image);
        return NULL;
    }
    return image;
}

void rgb565_image_free(rgb565_image_t* image)
{
    if (image)
    {
        if (image->pixels)
            free(image->pixels);
        free(image);
    }
}

int bgr_image_to_rgb565(const image_t* src, rgb565_image_t* dst)
{
    if (!src || !dst)
    {
        return -1;
    }
    if (src->width * src->height != dst->size)
    {
        return -1;
    }
    for (size_t i = 0; i < src->width * src->height; i++)
    {
        dst->pixels[i].r = src->pixels[i].bgr.r >> 3;
        dst->pixels[i].g = src->pixels[i].bgr.g >> 2;
        dst->pixels[i].b = src->pixels[i].bgr.b >> 3;
    }
    return 0;
}
