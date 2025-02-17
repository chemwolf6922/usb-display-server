cmake_minimum_required(VERSION 3.10)

project(usb-display-server)

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

add_executable(usb-screen-server
    main.c
    usb_screen.c
    ../../common/bmp.c
    ../../common/image.c
    ../../common/color_conversion.c
    ../../common/k_means_compression.c)

target_link_libraries(usb-screen-server
    tev
    m)
