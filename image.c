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
    image->color_space = COLOR_SPACE_bgr;
    image->pixels = (pixel_t*)malloc(width * height * sizeof(pixel_t));
    if (!image->pixels)
    {
        free(image);
        return NULL;
    }
    return image;
}

void free_image(image_t* image)
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