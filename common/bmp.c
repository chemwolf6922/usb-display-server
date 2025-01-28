#include "bmp.h"
#include <stdio.h>
#include <string.h>

image_t* load_24bit_bmp(const char* filename)
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

    image = image_new(dib_header.width, dib_header.height);
    /** Read pixels with padding */
    size_t row_size = BMP_24BIT_ROW_SIZE(image->width);
    size_t padding = row_size - image->width * 3;
    size_t offset = bmp_header.offset;
    fseek(file, offset, SEEK_SET);
    for (int y = (int)image->height - 1; y >= 0; y--)
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
    image_free(image);
    return NULL;
}

int load_24bit_bmp_from_ram(const uint8_t* data, size_t size, image_t* image)
{
    if (size < sizeof(bmp_header_t) + sizeof(dib_header_t))
    {
        fprintf(stderr, "Invalid BMP file size\n");
        return -1;
    }
    bmp_header_t bmp_header = {0};
    memcpy(&bmp_header, data, sizeof(bmp_header_t));
    if (bmp_header.offset != sizeof(bmp_header_t) + sizeof(dib_header_t))
    {
        /** Not supported */
        fprintf(stderr, "Unsupported BMP format (BMP header)\n");
        return -1;
    }
    dib_header_t dib_header = {0};
    memcpy(&dib_header, data + sizeof(bmp_header_t), sizeof(dib_header_t));
    if (dib_header.bit_count != 24 || dib_header.compression != 0)
    {
        /** Not supported */
        fprintf(stderr, "Unsupported BMP format (DIB header)\n");
        return -1;
    }
    if (size < dib_header.size_image + bmp_header.offset)
    {
        fprintf(stderr, "Invalid BMP file size\n");
        return -1;
    }

    /** Read pixels with padding */
    size_t row_size = BMP_24BIT_ROW_SIZE(image->width);
    size_t padding = row_size - image->width * 3;
    size_t offset = bmp_header.offset;
    for (int y = (int)image->height - 1; y >= 0; y--)
    {
        if (offset + image->width * sizeof(pixel_t) > size)
        {
            fprintf(stderr, "Cannot read pixels\n");
            return -1;
        }
        memcpy(&image->pixels[y * image->width], data + offset, image->width * sizeof(pixel_t));
        offset += row_size;
    }
    return 0;
}

void dump_image_to_bmp(const char* filename, const image_t* image)
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
    size_t row_size = BMP_24BIT_ROW_SIZE(image->width);
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
    for (int y = (int)image->height - 1; y >= 0; y--)
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
