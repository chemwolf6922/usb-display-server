#pragma once

#include "../../common/image.h"

/** These values should sync with the MCU firmware */
#define CONST_SCREEN_WIDTH (160)
#define CONST_SCREEN_HEIGHT (80)
#define CONST_N_COLOR (32)
#define CONST_FB_SIZE (CONST_SCREEN_WIDTH * CONST_SCREEN_HEIGHT * sizeof(pixel_t))

/** These are default values */
#define DEFAULT_SOCK_PATH "@usb-screen-server"
/** DO NOT set this too high. Set this to < 33 to better support  30fps video */
#define DEFAULT_FRAME_MIN_INTERVAL (30)
