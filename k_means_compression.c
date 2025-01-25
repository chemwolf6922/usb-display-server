#include "k_means_compression.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

static int inline get_closest_center(const ycbcr_pixel_t* pixel, const center_t* centers, int k, float* distance);

int k_means_compression(image_t* image, int k)
{
    double last_error = INFINITY;
    double error_thres = ERROR_THRES_PER_PIXEL * image->width * image->height;
    center_t* centers = (center_t*)malloc(k * sizeof(center_t));
    if (!centers)
    {
        return -1;
    }
    int* cluster_index = (int*)malloc(image->width * image->height * sizeof(int));
    if (!cluster_index)
    {
        free(centers);
        return -1;
    }
    /** Initilize the centers with random pixels from the image */
    for (int i = 0; i < k; i++)
    {
        size_t index = rand() % (image->width * image->height);
        pixel_t* pixel = &image->pixels[index];
        centers[i].y = pixel->ycbcr.y;
        centers[i].cb = pixel->ycbcr.cb;
        centers[i].cr = pixel->ycbcr.cr;
    }
    int iteration = 0;
    for(;;)
    {
        double error = 0;
        /** Calculate the cluster index and error for each pixel */
        for (size_t i = 0; i < image->width * image->height; i++)
        {
            /** Keep this loop simple to maximize vectorization */
            float min_distance = INFINITY;
            cluster_index[i] = get_closest_center(&image->pixels[i].ycbcr, centers, k, &min_distance);
            error += min_distance;
        }
        /** Check for exit condition */
        if (fabs(last_error - error) < error_thres)
        {
            break;
        }
        last_error = error;
        /** Calculate the new centers */
        memset(centers, 0, k * sizeof(center_t));
        for (size_t i = 0; i < image->width * image->height; i++)
        {
            centers[cluster_index[i]].y_sum += image->pixels[i].ycbcr.y;
            centers[cluster_index[i]].cb_sum += image->pixels[i].ycbcr.cb;
            centers[cluster_index[i]].cr_sum += image->pixels[i].ycbcr.cr;
            centers[cluster_index[i]].count++;
        }
        for (int i = 0; i < k; i++)
        {
            if (centers[i].count != 0)
            {
                centers[i].y = centers[i].y_sum / centers[i].count;
                centers[i].cb = centers[i].cb_sum / centers[i].count;
                centers[i].cr = centers[i].cr_sum / centers[i].count;
            }
        }
        iteration++;
    }
    /** Re paint the image with the new centers */
    for (int i = 0; i < k; i++)
    {
        centers[i].y = roundf(centers[i].y);
        centers[i].cb = roundf(centers[i].cb);
        centers[i].cr = roundf(centers[i].cr);
    }
    for (size_t i = 0; i < image->width * image->height; i++)
    {
        image->pixels[i].ycbcr.y = centers[cluster_index[i]].y;
        image->pixels[i].ycbcr.cb = centers[cluster_index[i]].cb;
        image->pixels[i].ycbcr.cr = centers[cluster_index[i]].cr;
    }
    free(cluster_index);
    free(centers);
    return iteration;
}

static inline __attribute__((__always_inline__)) float pixel_center_distance(const ycbcr_pixel_t* a, const center_t* b)
{
    /** Use float instead of double to increase vectorization */
    return sqrtf(powf((float)a->y - b->y, 2) + powf((float)a->cb - b->cb, 2) + powf((float)a->cr - b->cr, 2));
}

static inline int get_closest_center(const ycbcr_pixel_t* pixel, const center_t* centers, int k, float* p_min_distance)
{
    int index = -1;
    float min_distance = INFINITY;
    for (int i = 0; i < k; i++)
    {
        float distance = pixel_center_distance(pixel, &centers[i]);
        if (distance < min_distance)
        {
            min_distance = distance;
            index = i;
        }
    }
    *p_min_distance = min_distance;
    return index;
}

