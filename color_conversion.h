#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "image.h"

void bgr_to_ycbcr(const bgr_pixel_t *src, ycbcr_pixel_t *dst);
void ycbcr_to_bgr(const ycbcr_pixel_t *src, bgr_pixel_t *dst);
void bgr_image_to_ycbcr(const image_t* bgr, image_t* ycbcr);
void ycbcr_image_to_bgr(const image_t* ycbcr, image_t* bgr);


#ifdef __cplusplus
}
#endif
