#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "image.h"

void bgr_image_to_ycbcr(const image_t* bgr, image_t* ycbcr);
void ycbcr_image_to_bgr(const image_t* ycbcr, image_t* bgr);

void palette_bgr_to_ycbcr(const color_palette_image_t* src, color_palette_image_t* dst);
void palette_ycbcr_to_bgr(const color_palette_image_t* src, color_palette_image_t* dst);


#ifdef __cplusplus
}
#endif
