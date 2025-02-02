cmake_minimum_required(VERSION 3.10)

project(usb-display-app)

set(CMAKE_VERBOSE_MAKEFILE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-function)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-march=native)
endif()

add_executable(usb-display-play-video
    usb_screen_play_video.c
    usb_screen_client.c
    ../../common/color_conversion.c
    ../../common/image.c
    ../../common/k_means_compression.c)

target_link_libraries(usb-display-play-video
    avcodec
    avformat
    avutil
    swscale
    m)

add_executable(usb-display-show-image
    usb_screen_show_image.c
    usb_screen_client.c
    ../../common/color_conversion.c
    ../../common/image.c
    ../../common/k_means_compression.c)

target_link_libraries(usb-display-show-image
    avcodec
    avformat
    avutil
    swscale
    m)

add_executable(usb-display-rtmp
    usb_screen_rtmp.c
    usb_screen_client.c
    ../../common/image.c
    ../../common/color_conversion.c
    ../../common/k_means_compression.c)

target_link_libraries(usb-display-rtmp
    avcodec
    avformat
    avutil
    swscale
    m)


