#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "../../common/color_conversion.h"
#include "../../common/k_means_compression.h"
#include "../../common/bmp.h"
#include "../../common/image.h"
#include "cpu_cycle_counter.h"

#define COLOR_PALETTE_SIZE 32

static int seed_random();
static int save_data_to_file(const char* filename, const void* data, size_t size);

int main(int argc, char const *argv[])
{
    seed_random();

    int cpu_counter = cpu_cycle_counter_open();
    image_t* original = load_24bit_bmp("../../resource/desktop.bmp");
    if (!original)
    {
        return 1;
    }
    image_t* dst = image_new(original->width, original->height);
    if (!dst)
    {
        return 1;
    }
    color_palette_image_t* compressed = color_palette_image_new(COLOR_PALETTE_SIZE, original->width, original->height);
    if (!compressed)
    {
        return 1;
    }

    if (cpu_counter >= 0)
    {
        cpu_cycle_counter_reset(cpu_counter);
    }
    bgr_image_to_ycbcr(original, original);
    if (cpu_counter >= 0)
    {
        long long cycles = cpu_cycle_counter_get_result(cpu_counter);
        printf("Color conversion took %lld cycles\n", cycles);
        printf("cycles per pixel: %f\n\n", (double)cycles/(original->width * original->height));
    }

    if (cpu_counter >= 0)
    {
        cpu_cycle_counter_reset(cpu_counter);
    }
    int iterations = k_means_compression(original, COLOR_PALETTE_SIZE, compressed, false);
    if (cpu_counter >= 0)
    {
        long long cycles = cpu_cycle_counter_get_result(cpu_counter);
        printf("K-means compression took %d iterations and %lld cycles\n", iterations, cycles);
        printf("cycles per iteration: %f\n", (double)cycles / iterations);
        printf("cycles per (pixel * iteration): %f\n", (double)cycles / (iterations * original->width * original->height));
    }

    paint_color_palette_image(compressed, dst);
    ycbcr_image_to_bgr(dst, dst);
    dump_image_to_bmp("../../output/desktop.bmp", dst);

    /** compress again with compressed as hint. */
    k_means_compression(original, COLOR_PALETTE_SIZE, compressed, true);
    paint_color_palette_image(compressed, dst);
    ycbcr_image_to_bgr(dst, dst);
    dump_image_to_bmp("../../output/desktop2.bmp", dst);

    /** pack compressed image */
    palette_ycbcr_to_bgr(compressed, compressed);
    packed_color_palette_image_t* packed_image = packed_color_palette_image_new(COLOR_PALETTE_SIZE, dst->width, dst->height);
    if (!packed_image)
    {
        return 1;
    }
    pack_color_palette_image(compressed, packed_image);
    save_data_to_file("../../output/desktop.packed", packed_image->data, packed_image->size);
    packed_color_palette_image_free(packed_image);

    color_palette_image_free(compressed);
    image_free(dst);
    image_free(original);
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

