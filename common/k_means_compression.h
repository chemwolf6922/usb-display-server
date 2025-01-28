#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "image.h"

int k_means_compression(const image_t* src, int k, color_palette_image_t* dst, bool use_dst_as_hint);

#ifdef __cplusplus
}
#endif
