#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "image.h"

void bgr_image_to_ycbcr(const image_t* bgr, image_t* ycbcr);
void ycbcr_image_to_bgr(const image_t* ycbcr, image_t* bgr);


#ifdef __cplusplus
}
#endif
