#include "color_conversion.h"
#include <math.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

/** ITU-R BT.709 conversion */


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

#if defined(__AVX2__)
inline static void bgr_to_ycbcr_batch(const pixel_t* src, pixel_t* dst, int n)
{
    int i = 0;
    for(i = 0; i + 8 < n; i+=8)
    {
        /** @todo this is not efficient */
        __m256 b = _mm256_setr_ps(
            (float)src[i].bgr.b,
            (float)src[i+1].bgr.b,
            (float)src[i+2].bgr.b,
            (float)src[i+3].bgr.b,
            (float)src[i+4].bgr.b,
            (float)src[i+5].bgr.b,
            (float)src[i+6].bgr.b,
            (float)src[i+7].bgr.b);
        __m256 g = _mm256_setr_ps(
            (float)src[i].bgr.g,
            (float)src[i+1].bgr.g,
            (float)src[i+2].bgr.g,
            (float)src[i+3].bgr.g,
            (float)src[i+4].bgr.g,
            (float)src[i+5].bgr.g,
            (float)src[i+6].bgr.g,
            (float)src[i+7].bgr.g);
        __m256 r = _mm256_setr_ps(
            (float)src[i].bgr.r,
            (float)src[i+1].bgr.r,
            (float)src[i+2].bgr.r,
            (float)src[i+3].bgr.r,
            (float)src[i+4].bgr.r,
            (float)src[i+5].bgr.r,
            (float)src[i+6].bgr.r,
            (float)src[i+7].bgr.r);
        __m256 y = _mm256_add_ps(_mm256_mul_ps(r, _mm256_set1_ps(0.2126f)),
            _mm256_add_ps(_mm256_mul_ps(g, _mm256_set1_ps(0.7152f)),
                _mm256_mul_ps(b, _mm256_set1_ps(0.0722f))));
        y = _mm256_round_ps(y, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        __m256 cb = _mm256_add_ps(_mm256_mul_ps(r, _mm256_set1_ps(-0.1146f)),
            _mm256_add_ps(_mm256_mul_ps(g, _mm256_set1_ps(-0.3854f)),
                _mm256_mul_ps(b, _mm256_set1_ps(0.5000f))));
        cb = _mm256_round_ps(cb, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        __m256 cr = _mm256_add_ps(_mm256_mul_ps(r, _mm256_set1_ps(0.5000f)),
            _mm256_add_ps(_mm256_mul_ps(g, _mm256_set1_ps(-0.4542f)),
                _mm256_mul_ps(b, _mm256_set1_ps(-0.0458f))));
        cr = _mm256_round_ps(cr, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        float buffer[8];
        _mm256_storeu_ps(buffer, y);
        dst[i].ycbcr.y = buffer[0];
        dst[i+1].ycbcr.y = buffer[1];
        dst[i+2].ycbcr.y = buffer[2];
        dst[i+3].ycbcr.y = buffer[3];
        dst[i+4].ycbcr.y = buffer[4];
        dst[i+5].ycbcr.y = buffer[5];
        dst[i+6].ycbcr.y = buffer[6];
        dst[i+7].ycbcr.y = buffer[7];
        _mm256_storeu_ps(buffer, cb);
        dst[i].ycbcr.cb = buffer[0];
        dst[i+1].ycbcr.cb = buffer[1];
        dst[i+2].ycbcr.cb = buffer[2];
        dst[i+3].ycbcr.cb = buffer[3];
        dst[i+4].ycbcr.cb = buffer[4];
        dst[i+5].ycbcr.cb = buffer[5];
        dst[i+6].ycbcr.cb = buffer[6];
        dst[i+7].ycbcr.cb = buffer[7];
        _mm256_storeu_ps(buffer, cr);
        dst[i].ycbcr.cr = buffer[0];
        dst[i+1].ycbcr.cr = buffer[1];
        dst[i+2].ycbcr.cr = buffer[2];
        dst[i+3].ycbcr.cr = buffer[3];
        dst[i+4].ycbcr.cr = buffer[4];
        dst[i+5].ycbcr.cr = buffer[5];
        dst[i+6].ycbcr.cr = buffer[6];
        dst[i+7].ycbcr.cr = buffer[7];
    }
    for(; i < n; i++)
    {
        bgr_pixel_t bgr = src[i].bgr;
        ycbcr_pixel_t* ycbcr = &dst[i].ycbcr;
        ycbcr->y = (uint8_t)roundf(0.2126f * bgr.r + 0.7152f * bgr.g + 0.0722f * bgr.b);
        ycbcr->cb = (int8_t)roundf(-0.1146f * bgr.r - 0.3854f * bgr.g + 0.5000f * bgr.b);
        ycbcr->cr = (int8_t)roundf(0.5000f * bgr.r - 0.4542f * bgr.g - 0.0458f * bgr.b);
    }
}
#else
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
#endif

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

