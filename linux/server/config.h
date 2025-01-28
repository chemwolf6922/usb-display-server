#pragma once

#include "../../common/bmp.h"

/** These values should sync with the MCU firmware */
#define CONST_SCREEN_WIDTH (160)
#define CONST_SCREEN_HEIGHT (80)
#define CONST_N_COLOR (64)
#define INPUT_BMP_SIZE BMP_24BIT_SIZE(CONST_SCREEN_WIDTH, CONST_SCREEN_HEIGHT)

/** These are default values */
#define DEFAULT_PIPE_PATH "/tmp/usb-screen-server"
/** DO NOT set this too high */
#define DEFAULT_FRAME_MIN_INTERVAL (1000 / 30)
