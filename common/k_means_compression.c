#include "k_means_compression.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

/**
 * Performance considerations:
 * Caching the distance is attempting. But since we are dealing with 160*80 pixels.
 * The cache will be too large to be effective.
 * GCC 13 does a pretty descent job with auto vectorization.
 * Cost is around 100 cycle/pixel*iteration on ZEN3.
 */

typedef struct
{
    float y;
    float cb;
    float cr;
    double y_sum;
    double cb_sum;
    double cr_sum;
    size_t count;
} center_t;

#define ERROR_THRES_PER_PIXEL 0.001

inline static double update_clusters(int k, center_t* centers, const image_t* image, color_palette_image_t* dst);
#ifdef __AVX2__
inline static double update_clusters_fast16(center_t* centers, const image_t* image, color_palette_image_t* dst);
#endif

int k_means_compression(const image_t* image, int k, color_palette_image_t* dst, bool use_dst_as_hint)
{
    if (!image || !dst || k <= 0)
    {
        return -1;
    }
    if (image->height != dst->height || image->width != dst->width || k != dst->k)
    {
        return -1;
    }

    double last_error = INFINITY;
    double error_thres = ERROR_THRES_PER_PIXEL * image->width * image->height;
    center_t* centers = (center_t*)malloc(k * sizeof(center_t));
    if (!centers)
    {
        return -1;
    }
    if (use_dst_as_hint)
    {
        /** Use dst as hint to stabilize the frames */
        for (int i = 0; i < k; i++)
        {
            centers[i].y = dst->color_palettes[i].ycbcr.y;
            centers[i].cb = dst->color_palettes[i].ycbcr.cb;
            centers[i].cr = dst->color_palettes[i].ycbcr.cr;
        }
    }
    else
    {
        /** Initialize the centers with random pixels from the image */
        for (int i = 0; i < k; i++)
        {
            size_t index = rand() % (image->width * image->height);
            pixel_t* pixel = &image->pixels[index];
            centers[i].y = pixel->ycbcr.y;
            centers[i].cb = pixel->ycbcr.cb;
            centers[i].cr = pixel->ycbcr.cr;
        }
    }
    int iteration = 0;
    for(;;)
    {
        /** clear center sum and counters */
        for (int i = 0; i < 16; i++)
        {
            centers[i].y_sum = 0;
            centers[i].cb_sum = 0;
            centers[i].cr_sum = 0;
            centers[i].count = 0;
        }

        double error = INFINITY;
#ifdef __AVX2__
        if (k == 16)
        {
            error = update_clusters_fast16(centers, image, dst);
        }
        else
        {
            error = update_clusters(k, centers, image, dst);
        }
#else
        error = update_clusters(k, centers, image, dst);
#endif

        /** Check for exit condition */
        if (fabs(last_error - error) < error_thres)
        {
            break;
        }
        last_error = error;

        /** Calculate the new centers */
        for (int i = 0; i < k; i++)
        {
            if (centers[i].count != 0)
            {
                centers[i].y = centers[i].y_sum / centers[i].count;
                centers[i].cb = centers[i].cb_sum / centers[i].count;
                centers[i].cr = centers[i].cr_sum / centers[i].count;
            }
            else
            {
                /** 
                 * Re initialize the empty center with a random point from the dataset.
                 * Ideally we should use the farthest point from the center of the largest group. 
                 */
                pixel_t random_pixel = image->pixels[rand()%(image->width * image->height)];
                centers[i].y = random_pixel.ycbcr.y;
                centers[i].cb = random_pixel.ycbcr.cb;
                centers[i].cr = random_pixel.ycbcr.cr;
            }
        }
        iteration++;
    }
    /** Re paint the image with the new centers */
    for (int i = 0; i < k; i++)
    {
        dst->color_palettes[i].ycbcr.y = roundf(centers[i].y);
        dst->color_palettes[i].ycbcr.cb = roundf(centers[i].cb);
        dst->color_palettes[i].ycbcr.cr = roundf(centers[i].cr);
    }
    free(centers);
    return iteration;
}

inline static double update_clusters(int k, center_t* centers, const image_t* image, color_palette_image_t* dst)
{
    double error = 0;
    /** Calculate the cluster index and error for each pixel */
    for (size_t i = 0; i < image->width * image->height; i++)
    {
        /** Keep this loop simple to maximize vectorization */
        float min_distance = INFINITY;
        int index = -1;
        ycbcr_pixel_t* pixel = &image->pixels[i].ycbcr;
        for (int j = 0; j < k; j++)
        {
            center_t* center = &centers[j];
            float distance = sqrtf(
                powf((float)pixel->y - center->y, 2) + 
                powf((float)pixel->cb - center->cb, 2) + 
                powf((float)pixel->cr - center->cr, 2));
            if (distance < min_distance)
            {
                min_distance = distance;
                index = j;
            }
        }
        dst->pixel_indexs[i] = index;
        error += min_distance;

        centers[dst->pixel_indexs[i]].y_sum += image->pixels[i].ycbcr.y;
        centers[dst->pixel_indexs[i]].cb_sum += image->pixels[i].ycbcr.cb;
        centers[dst->pixel_indexs[i]].cr_sum += image->pixels[i].ycbcr.cr;
        centers[dst->pixel_indexs[i]].count++;
    }
    return error;
}

#if defined(__AVX512F__)
inline static double update_clusters_fast16(center_t* centers, const image_t* image, color_palette_image_t* dst)
{
    double error = 0;
    /** prepare center vectors */
    __m512 y_c = _mm512_set_ps(
        centers[15].y, centers[14].y, centers[13].y, centers[12].y,
        centers[11].y, centers[10].y, centers[9].y, centers[8].y,
        centers[7].y, centers[6].y, centers[5].y, centers[4].y,
        centers[3].y, centers[2].y, centers[1].y, centers[0].y);
    __m512 cb_c = _mm512_set_ps(
        centers[15].cb, centers[14].cb, centers[13].cb, centers[12].cb,
        centers[11].cb, centers[10].cb, centers[9].cb, centers[8].cb,
        centers[7].cb, centers[6].cb, centers[5].cb, centers[4].cb,
        centers[3].cb, centers[2].cb, centers[1].cb, centers[0].cb);
    __m512 cr_c = _mm512_set_ps(
        centers[15].cr, centers[14].cr, centers[13].cr, centers[12].cr,
        centers[11].cr, centers[10].cr, centers[9].cr, centers[8].cr,
        centers[7].cr, centers[6].cr, centers[5].cr, centers[4].cr,
        centers[3].cr, centers[2].cr, centers[1].cr, centers[0].cr);

    /** Calculate the cluster index and error for each pixel */
    for (size_t i = 0; i < image->width * image->height; i++)
    {
        __m512 y = _mm512_set1_ps(image->pixels[i].ycbcr.y);
        __m512 cb = _mm512_set1_ps(image->pixels[i].ycbcr.cb);
        __m512 cr = _mm512_set1_ps(image->pixels[i].ycbcr.cr);

        __m512 y_diff = _mm512_sub_ps(y_c, y);
        y_diff = _mm512_mul_ps(y_diff, y_diff);
        __m512 cb_diff = _mm512_sub_ps(cb_c, cb);
        cb_diff = _mm512_mul_ps(cb_diff, cb_diff);
        __m512 cr_diff = _mm512_sub_ps(cr_c, cr);
        cr_diff = _mm512_mul_ps(cr_diff, cr_diff);
        __m512 distance = _mm512_add_ps(y_diff, cb_diff);
        distance = _mm512_add_ps(distance, cr_diff);
        /** We can calculate the sqrt after we found the minimum value */

        /** Find the minimum value and index in distance_lo and distance_hi */
        float min_distance = _mm512_reduce_min_ps(distance);
        int mask = _mm512_cmpeq_ps_mask(distance, _mm512_set1_ps(min_distance));
        int index = 31 - __builtin_clz(mask);

        dst->pixel_indexs[i] = index;
        error += sqrtf(min_distance);

        centers[index].y_sum += image->pixels[i].ycbcr.y;
        centers[index].cb_sum += image->pixels[i].ycbcr.cb;
        centers[index].cr_sum += image->pixels[i].ycbcr.cr;
        centers[index].count++;
    }
    return error;
}
#elif defined(__AVX2__)
inline static double update_clusters_fast16(center_t* centers, const image_t* image, color_palette_image_t* dst)
{
    double error = 0;
    /** clear center sum and counters */
    for (int i = 0; i < 16; i++)
    {
        centers[i].y_sum = 0;
        centers[i].cb_sum = 0;
        centers[i].cr_sum = 0;
        centers[i].count = 0;
    }

    /** prepare center vectors */
    
    __m256 y_lo = _mm256_set_ps(
        centers[7].y, centers[6].y, centers[5].y, centers[4].y,
        centers[3].y, centers[2].y, centers[1].y, centers[0].y);
    __m256 y_hi = _mm256_set_ps(
        centers[15].y, centers[14].y, centers[13].y, centers[12].y,
        centers[11].y, centers[10].y, centers[9].y, centers[8].y);
    __m256 cb_lo = _mm256_set_ps(
        centers[7].cb, centers[6].cb, centers[5].cb, centers[4].cb,
        centers[3].cb, centers[2].cb, centers[1].cb, centers[0].cb);
    __m256 cb_hi = _mm256_set_ps(
        centers[15].cb, centers[14].cb, centers[13].cb, centers[12].cb,
        centers[11].cb, centers[10].cb, centers[9].cb, centers[8].cb);
    __m256 cr_lo = _mm256_set_ps(
        centers[7].cr, centers[6].cr, centers[5].cr, centers[4].cr,
        centers[3].cr, centers[2].cr, centers[1].cr, centers[0].cr);
    __m256 cr_hi = _mm256_set_ps(
        centers[15].cr, centers[14].cr, centers[13].cr, centers[12].cr,
        centers[11].cr, centers[10].cr, centers[9].cr, centers[8].cr);

    /** Calculate the cluster index and error for each pixel */
    for (size_t i = 0; i < image->width * image->height; i++)
    {
        __m256 y = _mm256_set1_ps(image->pixels[i].ycbcr.y);
        __m256 cb = _mm256_set1_ps(image->pixels[i].ycbcr.cb);
        __m256 cr = _mm256_set1_ps(image->pixels[i].ycbcr.cr);

        __m256 y_diff = _mm256_sub_ps(y_lo, y);
        y_diff = _mm256_mul_ps(y_diff, y_diff);
        __m256 cb_diff = _mm256_sub_ps(cb_lo, cb);
        cb_diff = _mm256_mul_ps(cb_diff, cb_diff);
        __m256 cr_diff = _mm256_sub_ps(cr_lo, cr);
        cr_diff = _mm256_mul_ps(cr_diff, cr_diff);
        __m256 distance_lo = _mm256_add_ps(y_diff, cb_diff);
        distance_lo = _mm256_add_ps(distance_lo, cr_diff);
        /** We can calculate the sqrt after we found the minimum value */

        y_diff = _mm256_sub_ps(y_hi, y);
        y_diff = _mm256_mul_ps(y_diff, y_diff);
        cb_diff = _mm256_sub_ps(cb_hi, cb);
        cb_diff = _mm256_mul_ps(cb_diff, cb_diff);
        cr_diff = _mm256_sub_ps(cr_hi, cr);
        cr_diff = _mm256_mul_ps(cr_diff, cr_diff);
        __m256 distance_hi = _mm256_add_ps(y_diff, cb_diff);
        distance_hi = _mm256_add_ps(distance_hi, cr_diff);

        /** Find the minimum value and index in distance_lo and distance_hi */
        __m256 distance = _mm256_min_ps(distance_lo, distance_hi);
        __m256 distance_fold = _mm256_permute2f128_ps(distance, distance, 1);
        distance = _mm256_min_ps(distance, distance_fold);
        distance_fold = _mm256_permute_ps(distance, 0b01001110);
        distance = _mm256_min_ps(distance, distance_fold);
        distance_fold = _mm256_permute_ps(distance, 0b10110001);
        /** At this moment, distance should be the min value * 8 */
        distance = _mm256_min_ps(distance, distance_fold);

        float min_distance[8];
        _mm256_storeu_ps(min_distance, distance);
        int index = -1;
    
        distance_hi = _mm256_cmp_ps(distance_hi, distance, _CMP_EQ_OQ);
        int mask_hi = _mm256_movemask_ps(distance_hi);
        if (mask_hi)
        {
            index = 8 + 31 - __builtin_clz(mask_hi);
        }
        else
        {
            distance_lo = _mm256_cmp_ps(distance_lo, distance, _CMP_EQ_OQ);
            int mask_lo = _mm256_movemask_ps(distance_lo);
            index = 31 - __builtin_clz(mask_lo);
        }

        dst->pixel_indexs[i] = index;
        error += sqrtf(min_distance[0]);

        centers[index].y_sum += image->pixels[i].ycbcr.y;
        centers[index].cb_sum += image->pixels[i].ycbcr.cb;
        centers[index].cr_sum += image->pixels[i].ycbcr.cr;
        centers[index].count++;
    }
    return error;
}
#endif


