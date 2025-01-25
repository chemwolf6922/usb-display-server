#include "k_means_compression.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct
{
    double y;
    double cb;
    double cr;
    size_t count;
} center_t;

#define ERROR_THRES_PER_PIXEL 0.01

static double pixel_center_distance(const ycbcr_pixel_t* a, const center_t* b);

int k_means_compression(image_t* image, int k)
{
    double last_error = INFINITY;
    double error_thres = ERROR_THRES_PER_PIXEL * image->width * image->height;
    center_t* centers = (center_t*)malloc(k * sizeof(center_t));
    if (!centers)
    {
        return -1;
    }
    memset(centers, 0, k * sizeof(center_t));
    int* cluster_index = (int*)malloc(image->width * image->height * sizeof(int));
    if (!cluster_index)
    {
        free(centers);
        return -1;
    }
    memset(cluster_index, 0, image->width * image->height * sizeof(int));
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
            double min_distance = INFINITY;
            for (int j = 0; j < k; j++)
            {
                double distance = pixel_center_distance(&image->pixels[i].ycbcr, &centers[j]);
                if (distance < min_distance)
                {
                    min_distance = distance;
                    cluster_index[i] = j;
                }
            }
            error += min_distance;
        }

        /** Debug */
        printf("Error: %f\n", error);
        printf("Delta: %f\n", error - last_error);
        printf("Iteration %d\n", iteration++);

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
            centers[cluster_index[i]].y += image->pixels[i].ycbcr.y;
            centers[cluster_index[i]].cb += image->pixels[i].ycbcr.cb;
            centers[cluster_index[i]].cr += image->pixels[i].ycbcr.cr;
            centers[cluster_index[i]].count++;
        }
        for (int i = 0; i < k; i++)
        {
            if (centers[i].count != 0)
            {
                centers[i].y = (centers[i].y / centers[i].count);
                centers[i].cb = (centers[i].cb / centers[i].count);
                centers[i].cr = (centers[i].cr / centers[i].count);
            }
        }

        for(int i = 0; i < k; i++)
        {
            printf("Center %d: Y: %.2lf, Cb: %.2lf, Cr: %.2lf\n", i, centers[i].y, centers[i].cb, centers[i].cr);
        }
    }
    /** Re paint the image with the new centers */
    for (size_t i = 0; i < image->width * image->height; i++)
    {
        image->pixels[i].ycbcr.y = centers[cluster_index[i]].y;
        image->pixels[i].ycbcr.cb = centers[cluster_index[i]].cb;
        image->pixels[i].ycbcr.cr = centers[cluster_index[i]].cr;
    }
    free(cluster_index);
    free(centers);
    return 0;
}

static double pixel_center_distance(const ycbcr_pixel_t* a, const center_t* b)
{
    double distance = sqrt(pow((double)a->y - b->y, 2) + pow((double)a->cb - b->cb, 2) + pow((double)a->cr - b->cr, 2));
    return distance;
}

