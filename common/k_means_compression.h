#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "image.h"

int k_means_compression(const image_t* src, int k, color_palette_image_t* dst);

#ifdef __cplusplus
}
#endif
