#pragma once

#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

image_t* load_24bit_bmp(const char* filename);
void dump_image_to_bmp(const char* filename, const image_t* image);

#ifdef __cplusplus
}
#endif
