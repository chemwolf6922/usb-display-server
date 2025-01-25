#include "color_conversion.h"
#include <math.h>

/** ITU-R BT.709 conversion */

static void inline __attribute__((__always_inline__)) bgr_to_ycbcr(const bgr_pixel_t *src, ycbcr_pixel_t *dst)
{
    dst->y = (uint8_t)roundf(0.2126f * src->r + 0.7152f * src->g + 0.0722f * src->b);
    dst->cb = (int8_t)roundf(-0.1146f * src->r - 0.3854f * src->g + 0.5000f * src->b);
    dst->cr = (int8_t)roundf(0.5000f * src->r - 0.4542f * src->g - 0.0458f * src->b);
}

static void inline __attribute__((__always_inline__)) ycbcr_to_bgr(const ycbcr_pixel_t *src, bgr_pixel_t *dst)
{
    dst->r = (uint8_t)(src->y + 1.5748 * src->cr);
    dst->g = (uint8_t)(src->y - 0.1873 * src->cb - 0.4681 * src->cr);
    dst->b = (uint8_t)(src->y + 1.8556 * src->cb);
}

void bgr_image_to_ycbcr(const image_t* bgr, image_t* ycbcr)
{
    if (bgr == ycbcr)
    {
        /** same image, needs temp value */
        ycbcr_pixel_t temp;
        for (size_t i = 0; i < bgr->width * bgr->height; i++)
        {
            bgr_to_ycbcr(&bgr->pixels[i].bgr, &temp);
            ycbcr->pixels[i].ycbcr = temp;
        }
    }
    else
    {
        for (size_t i = 0; i < bgr->width * bgr->height; i++)
        {
            bgr_to_ycbcr(&bgr->pixels[i].bgr, &ycbcr->pixels[i].ycbcr);
        }
    }
    ycbcr->color_space = COLOR_SPACE_YCBCR;
}

void ycbcr_image_to_bgr(const image_t* ycbcr, image_t* bgr)
{
    if (ycbcr == bgr)
    {
        /** same image, needs temp value */
        bgr_pixel_t temp;
        for (size_t i = 0; i < ycbcr->width * ycbcr->height; i++)
        {
            ycbcr_to_bgr(&ycbcr->pixels[i].ycbcr, &temp);
            bgr->pixels[i].bgr = temp;
        }
    }
    else
    {
        for (size_t i = 0; i < ycbcr->width * ycbcr->height; i++)
        {
            ycbcr_to_bgr(&ycbcr->pixels[i].ycbcr, &bgr->pixels[i].bgr);
        }
    }
    bgr->color_space = COLOR_SPACE_bgr;
}
