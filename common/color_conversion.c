#include "color_conversion.h"
#include <math.h>

/** ITU-R BT.709 conversion */
inline static void bgr_to_ycbcr_batch(const pixel_t* src, pixel_t* dst, int n)
{
    for (int i = 0; i < n; i++)
    {
        /** Make a copy to enable in place conversion */
        bgr_pixel_t bgr = src[i].bgr;
        ycbcr_pixel_t* ycbcr = &dst[i].ycbcr;
        ycbcr->y = (uint8_t)roundf(0.2126f * bgr.r + 0.7152f * bgr.g + 0.0722f * bgr.b);
        ycbcr->cb = (int8_t)roundf(-0.1146f * bgr.r - 0.3854f * bgr.g + 0.5000f * bgr.b);
        ycbcr->cr = (int8_t)roundf(0.5000f * bgr.r - 0.4542f * bgr.g - 0.0458f * bgr.b);
    }
}

inline static void ycbcr_to_bgr_batch(const pixel_t* src, pixel_t* dst, int n)
{
    for (int i = 0; i < n; i++)
    {
        ycbcr_pixel_t ycbcr = src[i].ycbcr;
        bgr_pixel_t* bgr = &dst[i].bgr;
        bgr->r = (uint8_t)(ycbcr.y + 1.5748 * ycbcr.cr);
        bgr->g = (uint8_t)(ycbcr.y - 0.1873 * ycbcr.cb - 0.4681 * ycbcr.cr);
        bgr->b = (uint8_t)(ycbcr.y + 1.8556 * ycbcr.cb);
    }
}

void bgr_image_to_ycbcr(const image_t* bgr, image_t* ycbcr)
{
    bgr_to_ycbcr_batch(bgr->pixels, ycbcr->pixels, bgr->width * bgr->height);
    ycbcr->color_space = COLOR_SPACE_YCBCR;
}

void ycbcr_image_to_bgr(const image_t* ycbcr, image_t* bgr)
{
    ycbcr_to_bgr_batch(ycbcr->pixels, bgr->pixels, ycbcr->width * ycbcr->height);
    bgr->color_space = COLOR_SPACE_BGR;
}

void palette_bgr_to_ycbcr(const color_palette_image_t* src, color_palette_image_t* dst)
{
    bgr_to_ycbcr_batch(src->color_palettes, dst->color_palettes, src->k);
    dst->color_space = COLOR_SPACE_YCBCR;
}

void palette_ycbcr_to_bgr(const color_palette_image_t* src, color_palette_image_t* dst)
{
    ycbcr_to_bgr_batch(src->color_palettes, dst->color_palettes, src->k);
    dst->color_space = COLOR_SPACE_BGR;
}

