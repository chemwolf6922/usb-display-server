
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "color_conversion.h"
#include "k_means_compression.h"

static image_t* load_24bit_bmp(const char* filename);
static image_t* image_new(size_t width, size_t height);
static void free_image(image_t* image);
static void dump_image_to_bmp(const char* filename, const image_t* image);

int main(int argc, char const *argv[])
{
    image_t* pic = load_24bit_bmp("./image/MountRainier24bit.bmp");
    if (!pic)
    {
        return 1;
    }
    bgr_image_to_ycbcr(pic, pic);
    k_means_compression(pic, 16);
    ycbcr_image_to_bgr(pic, pic);
    dump_image_to_bmp("./output/MountRainier4bit.bmp", pic);
    free_image(pic);
    return 0;
}

typedef struct
{
    char signature[2];
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} __attribute__((packed)) bmp_header_t;

typedef struct
{
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t size_image;
    int32_t x_pels_per_meter;
    int32_t y_pels_per_meter;
    uint32_t clr_used;
    uint32_t clr_important;
} __attribute__((packed)) dib_header_t;

static image_t* load_24bit_bmp(const char* filename)
{
    image_t* image = NULL;
    FILE* file = NULL;

    file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "Cannot open file %s\n", filename);
        goto error;
    }

    bmp_header_t bmp_header = {0};
    if (fread(&bmp_header, 1, sizeof(bmp_header_t), file) != sizeof(bmp_header_t))
    {
        fprintf(stderr, "Cannot read BMP header\n");
        goto error;
    }
    if (bmp_header.offset != sizeof(bmp_header_t) + sizeof(dib_header_t))
    {
        /** Not supported */
        fprintf(stderr, "Unsupported BMP format (BMP header)\n");
        goto error;
    }
    dib_header_t dib_header = {0};
    if (fread(&dib_header, 1, sizeof(dib_header_t), file) != sizeof(dib_header_t))
    {
        fprintf(stderr, "Cannot read DIB header\n");
        goto error;
    }
    if (dib_header.bit_count != 24 || dib_header.compression != 0)
    {
        /** Not supported */
        fprintf(stderr, "Unsupported BMP format (DIB header)\n");
        goto error;
    }

    image = (image_t*)malloc(sizeof(image_t));
    if (!image)
    {
        fprintf(stderr, "Cannot allocate memory\n");
        goto error;
    }
    image->width = dib_header.width;
    image->height = dib_header.height;
    image->color_space = COLOR_SPACE_bgr;
    image->pixels = (pixel_t*)malloc(image->width * image->height * sizeof(pixel_t));
    if (!image->pixels)
    {
        fprintf(stderr, "Cannot allocate memory\n");
        goto error;
    }
    /** Read pixels with padding */
    size_t row_size = (image->width * 3 + 3) / 4 * 4;
    size_t padding = row_size - image->width * 3;
    size_t offset = bmp_header.offset;
    fseek(file, offset, SEEK_SET);
    for (size_t y = 0; y < image->height; y++)
    {
        if (fread(&image->pixels[y * image->width], 1, image->width * sizeof(pixel_t), file) != image->width * sizeof(pixel_t))
        {
            fprintf(stderr, "Cannot read pixels\n");
            goto error;
        }
        fseek(file, padding, SEEK_CUR);
    }
    fclose(file);
    return image;
error:
    if (file)
    {
        fclose(file);
    }
    free_image(image);
    return NULL;
}

static image_t* image_new(size_t width, size_t height)
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

static void free_image(image_t* image)
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

static void dump_image_to_bmp(const char* filename, const image_t* image)
{
    FILE* file = fopen(filename, "wb");
    if (!file)
    {
        fprintf(stderr, "Cannot open file %s\n", filename);
        return;
    }
    bmp_header_t bmp_header = {0};
    bmp_header.signature[0] = 'B';
    bmp_header.signature[1] = 'M';
    size_t row_size = (image->width * 3 + 3) / 4 * 4;
    bmp_header.size = sizeof(bmp_header_t) + sizeof(dib_header_t) + row_size * image->height;
    bmp_header.offset = sizeof(bmp_header_t) + sizeof(dib_header_t);
    fwrite(&bmp_header, 1, sizeof(bmp_header_t), file);
    dib_header_t dib_header = {0};
    dib_header.size = sizeof(dib_header_t);
    dib_header.width = image->width;
    dib_header.height = image->height;
    dib_header.planes = 1;
    dib_header.bit_count = 24;
    dib_header.size_image = row_size * image->height;
    fwrite(&dib_header, 1, sizeof(dib_header_t), file);
    size_t padding = row_size - image->width * 3;
    for (size_t y = 0; y < image->height; y++)
    {
        fwrite(&image->pixels[y * image->width], 1, image->width * sizeof(pixel_t), file);
        /** Max padding size is 4 */
        uint8_t zeroPadding[4] = {0};
        if (padding > 0)
        {
            fwrite(zeroPadding, 1, padding, file);
        }
    }
    fclose(file);
}