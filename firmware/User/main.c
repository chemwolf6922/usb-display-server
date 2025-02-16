/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/12/26
 * Description        : Main program body.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#include "ch32x035_conf.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "ch32x035_usbfs_device.h"
#include "lcd.h"
#include "../../../common/config.h"

#if FRAME_COMPRESSION == FRAME_COMPRESSION_NONE
/** 5 packet is 1 line */
#define BUFFER_PACKET_COUNT (5 * 20)
#elif FRAME_COMPRESSION == FRAME_COMPRESSION_K_MEANS
#define BUFFER_PACKET_COUNT (((IMAGE_SIZE + 63) / 64))
#endif

static volatile atomic_bool image_ready = false;

static struct
{
#if FRAME_COMPRESSION == FRAME_COMPRESSION_NONE
    __attribute__((aligned(4))) uint8_t buffer_A[64 * BUFFER_PACKET_COUNT];
    __attribute__((aligned(4))) uint8_t buffer_B[64 * BUFFER_PACKET_COUNT];
    uint8_t* cdc_buffer;
    int write_offset;
    int image_bytes_written;
#elif FRAME_COMPRESSION == FRAME_COMPRESSION_K_MEANS
    __attribute__((aligned(4))) uint8_t cdc_buffer[64 * BUFFER_PACKET_COUNT];
    int write_offset;
    color_t color_palette[COLOR_PALETTE_SIZE];
#endif
    lcd_t lcd;
} app;

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    systick_init();

#if FRAME_COMPRESSION == FRAME_COMPRESSION_NONE
    app.cdc_buffer = app.buffer_A;
#endif

    /* Init serial number */
    uint32_t chip_id = DBGMCU_GetCHIPID();
    char chip_id_str[20] = {0};
    sprintf(chip_id_str, "%"PRIu32, chip_id);
    usb_desc_set_serial_number(chip_id_str);

    /** RCC init */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE );
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE );
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, ENABLE);

    /* Usb Init */
    app.write_offset = 0;
    USBFS_Device_Init( ENABLE , PWR_VDD_SupplyVoltage());

    /** LCD init */
    app.lcd.spi_prescaler = SPI_BaudRatePrescaler_2;
    app.lcd.spi = SPI1;
    app.lcd.clk.group = GPIOA;
    app.lcd.clk.pin = GPIO_Pin_5;
    app.lcd.mosi.group = GPIOA;
    app.lcd.mosi.pin = GPIO_Pin_7;
    app.lcd.dc.group = GPIOB;
    app.lcd.dc.pin = GPIO_Pin_0;
    app.lcd.ncs.group = GPIOB;
    app.lcd.ncs.pin = GPIO_Pin_3;
    app.lcd.nrst.group = GPIOB;
    app.lcd.nrst.pin = GPIO_Pin_1;
    lcd_init(&app.lcd);

    while(1)
    {
        /** Well, WFI does not work as expected. So busy loop it is. */
        while(!image_ready);
        image_ready = false;
#if FRAME_COMPRESSION == FRAME_COMPRESSION_NONE
        if (app.image_bytes_written == 0)
        {
            lcd_start_image_draw(&app.lcd);
        }
        uint8_t* buffer = app.cdc_buffer == app.buffer_A ? app.buffer_B : app.buffer_A;
        lcd_write_image_data(&app.lcd, buffer, sizeof(app.buffer_A));
        app.image_bytes_written += sizeof(app.buffer_A);
        if (app.image_bytes_written == IMAGE_HEIGHT * IMAGE_WIDTH * sizeof(color_t))
        {
            lcd_end_image_draw(&app.lcd);
            app.image_bytes_written = 0;
        }
#elif FRAME_COMPRESSION == FRAME_COMPRESSION_K_MEANS
        lcd_draw_image(
            &app.lcd,
            app.cdc_buffer + sizeof(app.color_palette),
            app.color_palette);
#endif
    }
}

uint8_t* __attribute__((section(".ramcode"))) cdc_hook_reset_rx_buffer()
{
    app.write_offset = 0;
#if FRAME_COMPRESSION == FRAME_COMPRESSION_NONE
    app.cdc_buffer = app.buffer_A;
#endif
    return app.cdc_buffer;
}


uint8_t* __attribute__((section(".ramcode"))) cdc_hook_on_data(int len)
{
#if FRAME_COMPRESSION == FRAME_COMPRESSION_NONE
    app.write_offset += len;
    if (app.write_offset == sizeof(app.buffer_A))
    {
        app.cdc_buffer = app.cdc_buffer == app.buffer_A ? app.buffer_B : app.buffer_A;
        app.write_offset = 0;
        image_ready = true;
    }
    return app.cdc_buffer + app.write_offset;
#elif FRAME_COMPRESSION == FRAME_COMPRESSION_K_MEANS
    app.write_offset += len;
    if (app.write_offset >= IMAGE_SIZE)
    {
        /** 
         * This logic is tightly tied to the image size.
         * A safer approach would be using a circular buffer.
         */

        /** copy the color palette to avoid overwritten */
        memcpy(app.color_palette, app.cdc_buffer, sizeof(app.color_palette));
        /** move the extra data to the top if any */
        if (app.write_offset > IMAGE_SIZE)
        {
            memmove(app.cdc_buffer, app.cdc_buffer + IMAGE_SIZE, app.write_offset - IMAGE_SIZE);
        }
        /** Switch buffer */
        app.write_offset -= IMAGE_SIZE;
        /** Label image as ready. So when execution resumes on the normal code flow, it can process the image. */
        image_ready = true;
    }
    return app.cdc_buffer + app.write_offset;
#endif
}
