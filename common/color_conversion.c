#include "color_conversion.h"
#include <math.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

/** ITU-R BT.709 conversion */

#if defined(__AVX512F__)
inline static void ycbcr_to_bgr_batch(const pixel_t* src, pixel_t* dst, int n)
{
    int i = 0;
    for(i = 0; i + 16 <= n; i+=16)
    {
        /** The lower 48 bytes holds 16 pixels */
        __m512i bytes = _mm512_maskz_loadu_epi8((1ull<<48ull)-1ull, &src[i]);
        /** load 16 bytes as 16 epi32 w/o sign expansion */
        /** endian: BE */
        __m512i y_i = _mm512_maskz_permutex2var_epi8(0x1111111111111111ull, bytes, _mm512_set_epi8(
            0,0,0,45,
            0,0,0,42,
            0,0,0,39,
            0,0,0,36,
            0,0,0,33,
            0,0,0,30,
            0,0,0,27,
            0,0,0,24,
            0,0,0,21,
            0,0,0,18,
            0,0,0,15,
            0,0,0,12,
            0,0,0,9,
            0,0,0,6,
            0,0,0,3,
            0,0,0,0
        ), bytes);
        __m512i cb_i = _mm512_maskz_permutex2var_epi8(0x000000000000FFFFull, bytes, _mm512_set_epi8(
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            46,43,40,37,
            34,31,28,25,
            22,19,16,13,
            10,7,4,1
        ), bytes);
        __m512i cr_i = _mm512_maskz_permutex2var_epi8(0x000000000000FFFFull, bytes, _mm512_set_epi8(
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            47,44,41,38,
            35,32,29,26,
            23,20,17,14,
            11,8,5,2
        ), bytes);
        __m512 y = _mm512_cvtepi32_ps(y_i);
        __m512 cb = _mm512_cvtepi32_ps(_mm512_cvtepi8_epi32(_mm512_castsi512_si128(cb_i)));
        __m512 cr = _mm512_cvtepi32_ps(_mm512_cvtepi8_epi32(_mm512_castsi512_si128(cr_i)));

        __m512 r = _mm512_add_ps(y,
            _mm512_mul_ps(cr, _mm512_set1_ps(1.5748f)));
        __m512 g = _mm512_add_ps(y,
            _mm512_add_ps(_mm512_mul_ps(cb, _mm512_set1_ps(-0.1873f)),
                _mm512_mul_ps(cr, _mm512_set1_ps(-0.4681f))));
        __m512 b = _mm512_add_ps(y,
            _mm512_mul_ps(cb, _mm512_set1_ps(1.8556f)));

        __m512i r_i = _mm512_cvt_roundps_epi32(r, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        __m512i g_i = _mm512_cvt_roundps_epi32(g, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        __m512i b_i = _mm512_cvt_roundps_epi32(b, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);

        /** @todo check endian */
        bytes = _mm512_permutex2var_epi8(b_i, _mm512_set_epi8(
            0x40 | 63, 0x40 | 62, 0x40 | 61, 0x40 | 60,
            0x40 | 59, 0x40 | 58, 0x40 | 57, 0x40 | 56,
            0x40 | 55, 0x40 | 54, 0x40 | 53, 0x40 | 52,
            0x40 | 51, 0x40 | 50, 0x40 | 49, 0x40 | 48,
            0x40 | 47, 0x40 | 46,        60, 0x40 | 44,
            0x40 | 43,        56, 0x40 | 41, 0x40 | 40,
                   52, 0x40 | 38, 0x40 | 37,        48,
            0x40 | 35, 0x40 | 34,        44, 0x40 | 32,
            0x40 | 31,        40, 0x40 | 29, 0x40 | 28,
                   36, 0x40 | 26, 0x40 | 25,        32,
            0x40 | 23, 0x40 | 22,        28, 0x40 | 20,
            0x40 | 19,        24, 0x40 | 17, 0x40 | 16,
                   20, 0x40 | 14, 0x40 | 13,        16,
            0x40 | 11, 0x40 | 10,        12, 0x40 |  8,
            0x40 |  7,         8, 0x40 |  5, 0x40 |  4,
                    4, 0x40 |  2, 0x40 |  1,         0
        ), bytes);
        bytes = _mm512_permutex2var_epi8(g_i, _mm512_set_epi8(
            0x40 | 63, 0x40 | 62, 0x40 | 61, 0x40 | 60,
            0x40 | 59, 0x40 | 58, 0x40 | 57, 0x40 | 56,
            0x40 | 55, 0x40 | 54, 0x40 | 53, 0x40 | 52,
            0x40 | 51, 0x40 | 50, 0x40 | 49, 0x40 | 48,
            0x40 | 47,        60, 0x40 | 45, 0x40 | 44,
                   56, 0x40 | 42, 0x40 | 41,        52,
            0x40 | 39, 0x40 | 38,        48, 0x40 | 36,
            0x40 | 35,        44, 0x40 | 33, 0x40 | 32,
                   40, 0x40 | 30, 0x40 | 29,        36,
            0x40 | 27, 0x40 | 26,        32, 0x40 | 24,
            0x40 | 23,        28, 0x40 | 21, 0x40 | 20,
                   24, 0x40 | 18, 0x40 | 17,        20,
            0x40 | 15, 0x40 | 14,        16, 0x40 | 12,
            0x40 | 11,        12, 0x40 |  9, 0x40 |  8,
                    8, 0x40 |  6, 0x40 |  5,         4,
            0x40 |  3, 0x40 |  2,         0, 0x40 |  0
        ), bytes);
        bytes = _mm512_permutex2var_epi8(r_i, _mm512_set_epi8(
            0x40 | 63, 0x40 | 62, 0x40 | 61, 0x40 | 60,
            0x40 | 59, 0x40 | 58, 0x40 | 57, 0x40 | 56,
            0x40 | 55, 0x40 | 54, 0x40 | 53, 0x40 | 52,
            0x40 | 51, 0x40 | 50, 0x40 | 49, 0x40 | 48,
                   60, 0x40 | 46, 0x40 | 45,        56,
            0x40 | 43, 0x40 | 42,        52, 0x40 | 40,
            0x40 | 39,        48, 0x40 | 37, 0x40 | 36,
                   44, 0x40 | 34, 0x40 | 33,        40,
            0x40 | 31, 0x40 | 30,        36, 0x40 | 28,
            0x40 | 27,        32, 0x40 | 25, 0x40 | 24,
                   28, 0x40 | 22, 0x40 | 21,        24,
            0x40 | 19, 0x40 | 18,        20, 0x40 | 16,
            0x40 | 15,        16, 0x40 | 13, 0x40 | 12,
                   12, 0x40 | 10, 0x40 |  9,         8,
            0x40 |  7, 0x40 |  6,         4, 0x40 |  4,
            0x40 |  3,         0, 0x40 |  1, 0x40 |  0
        ), bytes);

        _mm512_mask_storeu_epi8(&dst[i], (1ull<<48ull)-1ull, bytes);
    }
    for(; i < n; i++)
    {
        ycbcr_pixel_t ycbcr = src[i].ycbcr;
        bgr_pixel_t* bgr = &dst[i].bgr;
        bgr->r = (uint8_t)(ycbcr.y + 1.5748 * ycbcr.cr);
        bgr->g = (uint8_t)(ycbcr.y - 0.1873 * ycbcr.cb - 0.4681 * ycbcr.cr);
        bgr->b = (uint8_t)(ycbcr.y + 1.8556 * ycbcr.cb);
    }
}
#elif defined(__AVX2__)
inline static void ycbcr_to_bgr_batch(const pixel_t* src, pixel_t* dst, int n)
{
    int i = 0;
    for(i = 0; i + 8 <= n; i+=8)
    {
        /** @todo this is not efficient */
        __m256 y = _mm256_setr_ps(
            (float)src[i].ycbcr.y,
            (float)src[i+1].ycbcr.y,
            (float)src[i+2].ycbcr.y,
            (float)src[i+3].ycbcr.y,
            (float)src[i+4].ycbcr.y,
            (float)src[i+5].ycbcr.y,
            (float)src[i+6].ycbcr.y,
            (float)src[i+7].ycbcr.y);
        __m256 cb = _mm256_setr_ps(
            (float)src[i].ycbcr.cb,
            (float)src[i+1].ycbcr.cb,
            (float)src[i+2].ycbcr.cb,
            (float)src[i+3].ycbcr.cb,
            (float)src[i+4].ycbcr.cb,
            (float)src[i+5].ycbcr.cb,
            (float)src[i+6].ycbcr.cb,
            (float)src[i+7].ycbcr.cb);
        __m256 cr = _mm256_setr_ps(
            (float)src[i].ycbcr.cr,
            (float)src[i+1].ycbcr.cr,
            (float)src[i+2].ycbcr.cr,
            (float)src[i+3].ycbcr.cr,
            (float)src[i+4].ycbcr.cr,
            (float)src[i+5].ycbcr.cr,
            (float)src[i+6].ycbcr.cr,
            (float)src[i+7].ycbcr.cr);
        __m256 r = _mm256_add_ps(y,
            _mm256_mul_ps(cr, _mm256_set1_ps(1.5748f)));
        r = _mm256_round_ps(r, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        __m256 g = _mm256_add_ps(y,
            _mm256_add_ps(_mm256_mul_ps(cb, _mm256_set1_ps(-0.1873f)),
                _mm256_mul_ps(cr, _mm256_set1_ps(-0.4681f))));
        g = _mm256_round_ps(g, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        __m256 b = _mm256_add_ps(y,
            _mm256_mul_ps(cb, _mm256_set1_ps(1.8556f)));
        b = _mm256_round_ps(b, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        float buffer[8];
        _mm256_storeu_ps(buffer, r);
        dst[i].bgr.r = buffer[0];
        dst[i+1].bgr.r = buffer[1];
        dst[i+2].bgr.r = buffer[2];
        dst[i+3].bgr.r = buffer[3];
        dst[i+4].bgr.r = buffer[4];
        dst[i+5].bgr.r = buffer[5];
        dst[i+6].bgr.r = buffer[6];
        dst[i+7].bgr.r = buffer[7];
        _mm256_storeu_ps(buffer, g);
        dst[i].bgr.g = buffer[0];
        dst[i+1].bgr.g = buffer[1];
        dst[i+2].bgr.g = buffer[2];
        dst[i+3].bgr.g = buffer[3];
        dst[i+4].bgr.g = buffer[4];
        dst[i+5].bgr.g = buffer[5];
        dst[i+6].bgr.g = buffer[6];
        dst[i+7].bgr.g = buffer[7];
        _mm256_storeu_ps(buffer, b);
        dst[i].bgr.b = buffer[0];
        dst[i+1].bgr.b = buffer[1];
        dst[i+2].bgr.b = buffer[2];
        dst[i+3].bgr.b = buffer[3];
        dst[i+4].bgr.b = buffer[4];
        dst[i+5].bgr.b = buffer[5];
        dst[i+6].bgr.b = buffer[6];
        dst[i+7].bgr.b = buffer[7];
    }
    for(; i < n; i++)
    {
        ycbcr_pixel_t ycbcr = src[i].ycbcr;
        bgr_pixel_t* bgr = &dst[i].bgr;
        bgr->r = (uint8_t)(ycbcr.y + 1.5748 * ycbcr.cr);
        bgr->g = (uint8_t)(ycbcr.y - 0.1873 * ycbcr.cb - 0.4681 * ycbcr.cr);
        bgr->b = (uint8_t)(ycbcr.y + 1.8556 * ycbcr.cb);
    }
}
#else
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
#endif

#if defined(__AVX512F__)
inline static void bgr_to_ycbcr_batch(const pixel_t* src, pixel_t* dst, int n)
{
    int i = 0;
    for(i = 0; i + 16 <= n; i+=16)
    {
        /** The lower 48 bytes holds 16 pixels */
        __m512i bytes = _mm512_maskz_loadu_epi8((1ull<<48ull)-1ull, &src[i]);
        /** load 16 bytes as 16 epi32 w/o sign expansion */
        /** endian: BE */
        __m512i b_i = _mm512_maskz_permutex2var_epi8(0x1111111111111111ull, bytes, _mm512_set_epi8(
            0,0,0,45,
            0,0,0,42,
            0,0,0,39,
            0,0,0,36,
            0,0,0,33,
            0,0,0,30,
            0,0,0,27,
            0,0,0,24,
            0,0,0,21,
            0,0,0,18,
            0,0,0,15,
            0,0,0,12,
            0,0,0,9,
            0,0,0,6,
            0,0,0,3,
            0,0,0,0
        ), bytes);
        __m512i g_i = _mm512_maskz_permutex2var_epi8(0x1111111111111111ull, bytes, _mm512_set_epi8(
            0,0,0,46,
            0,0,0,43,
            0,0,0,40,
            0,0,0,37,
            0,0,0,34,
            0,0,0,31,
            0,0,0,28,
            0,0,0,25,
            0,0,0,22,
            0,0,0,19,
            0,0,0,16,
            0,0,0,13,
            0,0,0,10,
            0,0,0,7,
            0,0,0,4,
            0,0,0,1
        ), bytes);
        __m512i r_i = _mm512_maskz_permutex2var_epi8(0x1111111111111111ull, bytes, _mm512_set_epi8(
            0,0,0,47,
            0,0,0,44,
            0,0,0,41,
            0,0,0,38,
            0,0,0,35,
            0,0,0,32,
            0,0,0,29,
            0,0,0,26,
            0,0,0,23,
            0,0,0,20,
            0,0,0,17,
            0,0,0,14,
            0,0,0,11,
            0,0,0,8,
            0,0,0,5,
            0,0,0,2
        ), bytes);
        __m512 b = _mm512_cvtepi32_ps(b_i);
        __m512 g = _mm512_cvtepi32_ps(g_i);
        __m512 r = _mm512_cvtepi32_ps(r_i);

        __m512 y = _mm512_add_ps(_mm512_mul_ps(r, _mm512_set1_ps(0.2126f)),
            _mm512_add_ps(_mm512_mul_ps(g, _mm512_set1_ps(0.7152f)),
                _mm512_mul_ps(b, _mm512_set1_ps(0.0722f))));
        __m512 cb = _mm512_add_ps(_mm512_mul_ps(r, _mm512_set1_ps(-0.1146f)),
            _mm512_add_ps(_mm512_mul_ps(g, _mm512_set1_ps(-0.3854f)),
                _mm512_mul_ps(b, _mm512_set1_ps(0.5000f))));
        __m512 cr = _mm512_add_ps(_mm512_mul_ps(r, _mm512_set1_ps(0.5000f)),
            _mm512_add_ps(_mm512_mul_ps(g, _mm512_set1_ps(-0.4542f)),
                _mm512_mul_ps(b, _mm512_set1_ps(-0.0458f))));

        __m512i y_i = _mm512_cvt_roundps_epi32(y, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        __m512i cb_i = _mm512_cvt_roundps_epi32(cb, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
        __m512i cr_i = _mm512_cvt_roundps_epi32(cr, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);

        /** @todo check endian */
        bytes = _mm512_permutex2var_epi8(y_i, _mm512_set_epi8(
            0x40 | 63, 0x40 | 62, 0x40 | 61, 0x40 | 60,
            0x40 | 59, 0x40 | 58, 0x40 | 57, 0x40 | 56,
            0x40 | 55, 0x40 | 54, 0x40 | 53, 0x40 | 52,
            0x40 | 51, 0x40 | 50, 0x40 | 49, 0x40 | 48,
            0x40 | 47, 0x40 | 46,        60, 0x40 | 44,
            0x40 | 43,        56, 0x40 | 41, 0x40 | 40,
                   52, 0x40 | 38, 0x40 | 37,        48,
            0x40 | 35, 0x40 | 34,        44, 0x40 | 32,
            0x40 | 31,        40, 0x40 | 29, 0x40 | 28,
                   36, 0x40 | 26, 0x40 | 25,        32,
            0x40 | 23, 0x40 | 22,        28, 0x40 | 20,
            0x40 | 19,        24, 0x40 | 17, 0x40 | 16,
                   20, 0x40 | 14, 0x40 | 13,        16,
            0x40 | 11, 0x40 | 10,        12, 0x40 |  8,
            0x40 |  7,         8, 0x40 |  5, 0x40 |  4,
                    4, 0x40 |  2, 0x40 |  1,         0
        ), bytes);
        bytes = _mm512_permutex2var_epi8(cb_i, _mm512_set_epi8(
            0x40 | 63, 0x40 | 62, 0x40 | 61, 0x40 | 60,
            0x40 | 59, 0x40 | 58, 0x40 | 57, 0x40 | 56,
            0x40 | 55, 0x40 | 54, 0x40 | 53, 0x40 | 52,
            0x40 | 51, 0x40 | 50, 0x40 | 49, 0x40 | 48,
            0x40 | 47,        60, 0x40 | 45, 0x40 | 44,
                   56, 0x40 | 42, 0x40 | 41,        52,
            0x40 | 39, 0x40 | 38,        48, 0x40 | 36,
            0x40 | 35,        44, 0x40 | 33, 0x40 | 32,
                   40, 0x40 | 30, 0x40 | 29,        36,
            0x40 | 27, 0x40 | 26,        32, 0x40 | 24,
            0x40 | 23,        28, 0x40 | 21, 0x40 | 20,
                   24, 0x40 | 18, 0x40 | 17,        20,
            0x40 | 15, 0x40 | 14,        16, 0x40 | 12,
            0x40 | 11,        12, 0x40 |  9, 0x40 |  8,
                    8, 0x40 |  6, 0x40 |  5,         4,
            0x40 |  3, 0x40 |  2,         0, 0x40 |  0
        ), bytes);
        bytes = _mm512_permutex2var_epi8(cr_i, _mm512_set_epi8(
            0x40 | 63, 0x40 | 62, 0x40 | 61, 0x40 | 60,
            0x40 | 59, 0x40 | 58, 0x40 | 57, 0x40 | 56,
            0x40 | 55, 0x40 | 54, 0x40 | 53, 0x40 | 52,
            0x40 | 51, 0x40 | 50, 0x40 | 49, 0x40 | 48,
                   60, 0x40 | 46, 0x40 | 45,        56,
            0x40 | 43, 0x40 | 42,        52, 0x40 | 40,
            0x40 | 39,        48, 0x40 | 37, 0x40 | 36,
                   44, 0x40 | 34, 0x40 | 33,        40,
            0x40 | 31, 0x40 | 30,        36, 0x40 | 28,
            0x40 | 27,        32, 0x40 | 25, 0x40 | 24,
                   28, 0x40 | 22, 0x40 | 21,        24,
            0x40 | 19, 0x40 | 18,        20, 0x40 | 16,
            0x40 | 15,        16, 0x40 | 13, 0x40 | 12,
                   12, 0x40 | 10, 0x40 |  9,         8,
            0x40 |  7, 0x40 |  6,         4, 0x40 |  4,
            0x40 |  3,         0, 0x40 |  1, 0x40 |  0
        ), bytes);

        _mm512_mask_storeu_epi8(&dst[i], (1ull<<48ull)-1ull, bytes);
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
#elif defined(__AVX2__)
inline static void bgr_to_ycbcr_batch(const pixel_t* src, pixel_t* dst, int n)
{
    int i = 0;
    for(i = 0; i + 8 <= n; i+=8)
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

