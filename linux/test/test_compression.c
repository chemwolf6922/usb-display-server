#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "../../common/color_conversion.h"
#include "../../common/k_means_compression.h"
#include "../../common/bmp.h"
#include "../../common/image.h"
#include "cpu_cycle_counter.h"

#define COLOR_PALETTE_SIZE 16

static int seed_random();
static int save_data_to_file(const char* filename, const void* data, size_t size);

int main(int argc, char const *argv[])
{
    seed_random();

    int cpu_counter = cpu_cycle_counter_open();
    image_t* pic = load_24bit_bmp("../../image/desktop.bmp");
    if (!pic)
    {
        return 1;
    }
    color_palette_image_t* dst = color_palette_image_new(COLOR_PALETTE_SIZE, pic->width, pic->height);
    if (!dst)
    {
        return 1;
    }
    bgr_image_to_ycbcr(pic, pic);
    if (cpu_counter >= 0)
    {
        cpu_cycle_counter_reset(cpu_counter);
    }
    int iterations = k_means_compression(pic, COLOR_PALETTE_SIZE, dst);
    if (cpu_counter >= 0)
    {
        long long cycles = cpu_cycle_counter_get_result(cpu_counter);
        printf("K-means compression took %d iterations and %lld cycles\n", iterations, cycles);
        printf("cycles per iteration: %f\n", (double)cycles / iterations);
        printf("cycles per (pixel * iteration): %f\n", (double)cycles / (iterations * pic->width * pic->height));
    }
    paint_color_palette_image(dst, pic);
    packed_color_palette_image_t* packed_image = packed_color_palette_image_new(COLOR_PALETTE_SIZE, pic->width, pic->height);
    if (!packed_image)
    {
        return 1;
    }
    palette_ycbcr_to_bgr(dst, dst);
    for (int i = 0; i < dst->k; i++)
    {
        printf("Color %d: R: %d, G: %d, B: %d\n", i, dst->color_palettes[i].bgr.r, dst->color_palettes[i].bgr.g, dst->color_palettes[i].bgr.b);
    }
    pack_color_palette_image(dst, packed_image);
    color_palette_image_free(dst);
    save_data_to_file("../../output/desktop.packed", packed_image->data, packed_image->size);
    packed_color_palette_image_free(packed_image);
    ycbcr_image_to_bgr(pic, pic);
    dump_image_to_bmp("../../output/desktop.bmp", pic);
    image_free(pic);
    close(cpu_counter);
    return 0;
}

static int seed_random()
{
    unsigned int seed = 0;
    FILE* file = fopen("/dev/urandom", "r");
    if (!file)
    {
        return -1;
    }
    if (fread(&seed, 1, sizeof(seed), file) != sizeof(seed))
    {
        fclose(file);
        return -1;
    }
    fclose(file);
    srand(seed);
    return 0;
}

static int save_data_to_file(const char* filename, const void* data, size_t size)
{
    FILE* file = fopen(filename, "w");
    if (!file)
    {
        return -1;
    }
    if (fwrite(data, 1, size, file) != size)
    {
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}

