#pragma once

#include <stdint.h>
#include "ch32x035_conf.h"
#include "../../../common/config.h"

#define PIXEL_BITS 5
#define IMAGE_WIDTH 160
#define IMAGE_HEIGHT 80
#define COLOR_PALETTE_SIZE 32
#define IMAGE_SIZE ((IMAGE_HEIGHT * IMAGE_WIDTH * 5 + 7)/8 + COLOR_PALETTE_SIZE * sizeof(uint16_t))

typedef union
{
    struct
    {
        uint16_t b:5;
        uint16_t g:6;
        uint16_t r:5;
    } __attribute__((packed)) color;
    uint16_t raw;
} color_t;

typedef struct
{
    GPIO_TypeDef* group;
    int pin;
} gpio_pin_t;

typedef struct
{
    int spi_prescaler;
    SPI_TypeDef* spi;
    gpio_pin_t clk;
    gpio_pin_t mosi;
    gpio_pin_t ncs;
    gpio_pin_t dc;
    gpio_pin_t nrst;
} lcd_t;

void lcd_init(const lcd_t* lcd);

#if FRAME_COMPRESSION == FRAME_COMPRESSION_NONE
void lcd_start_image_draw(const lcd_t* lcd);
void lcd_write_image_data(const lcd_t* lcd, const uint8_t* data, int data_len);
void lcd_end_image_draw(const lcd_t* lcd);
#elif FRAME_COMPRESSION == FRAME_COMPRESSION_K_MEANS
void lcd_draw_image(
    const lcd_t* lcd,
    const uint8_t* indexs,
    const color_t* palette);
#endif

