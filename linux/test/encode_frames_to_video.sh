#!/bin/bash

rm -f ../../output/compressed.mp4

ffmpeg \
    -framerate 30 \
    -i ../../output/bbb_sunflower_compressed_frames/%d.bmp \
    -c:v libx264 \
    -pix_fmt yuv444p \
    ../../output/compressed.mp4
