#include <stdio.h>
#include <unistd.h>
#include "../../common/color_conversion.h"
#include "../../common/k_means_compression.h"
#include "../../common/bmp.h"
#include "../../common/image.h"
#include "cpu_cycle_counter.h"

int main(int argc, char const *argv[])
{
    int cpu_counter = cpu_cycle_counter_open();
    image_t* pic = load_24bit_bmp("../../image/desktop.bmp");
    if (!pic)
    {
        return 1;
    }
    bgr_image_to_ycbcr(pic, pic);
    if (cpu_counter >= 0)
    {
        cpu_cycle_counter_reset(cpu_counter);
    }
    int iterations = k_means_compression(pic, 16);
    if (cpu_counter >= 0)
    {
        long long cycles = cpu_cycle_counter_get_result(cpu_counter);
        printf("K-means compression took %d iterations and %lld cycles\n", iterations, cycles);
        printf("cycles per iteration: %f\n", (double)cycles / iterations);
        printf("cycles per (pixel * iteration): %f\n", (double)cycles / (iterations * pic->width * pic->height));
    }
    ycbcr_image_to_bgr(pic, pic);
    dump_image_to_bmp("../../output/desktop.bmp", pic);
    free_image(pic);
    close(cpu_counter);
    return 0;
}

