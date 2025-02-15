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

/** The compressed image data is 6400+32 bytes = 100.5 packets. */
#define PIXEL_BITS 5
#define IMAGE_WIDTH 160
#define IMAGE_HEIGHT 80
#define COLOR_PALETTE_SIZE 32
#define IMAGE_SIZE ((IMAGE_HEIGHT * IMAGE_WIDTH * 5 + 7)/8 + COLOR_PALETTE_SIZE * sizeof(uint16_t))
#define BUFFER_PACKET_COUNT (((IMAGE_SIZE + 63) / 64))

static volatile atomic_bool image_ready = false;

static struct
{
    __attribute__((aligned(4))) uint8_t cdc_buffer[64 * BUFFER_PACKET_COUNT];
    int write_offset;
    uint16_t color_palette[COLOR_PALETTE_SIZE];
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

    /* Init serial number */
    uint32_t chip_id = DBGMCU_GetCHIPID();
    char chip_id_str[20] = {0};
    sprintf(chip_id_str, "%"PRIu32, chip_id);
    usb_desc_set_serial_number(chip_id_str);

    /** RCC init */
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE );
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_DMA1, ENABLE );
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, ENABLE);

    /* Usb Init */
    app.write_offset = 0;
    USBFS_Device_Init( ENABLE , PWR_VDD_SupplyVoltage());

    while(1)
    {
        /** Well, WFI does not work as expected. So busy loop it is. */
        while(!image_ready);
        image_ready = false;
        /** Use uint32_t to load faster */
        uint32_t* pixels = (uint32_t*)(app.cdc_buffer + COLOR_PALETTE_SIZE * sizeof(uint16_t));
        /** @todo display image */
        (void)pixels;
    }
}

uint8_t* __attribute__((section(".ramcode"))) cdc_hook_reset_rx_buffer()
{
    app.write_offset = 0;
    return app.cdc_buffer;
}


uint8_t* __attribute__((section(".ramcode"))) cdc_hook_on_data(int len)
{
    app.write_offset += len;
    if (app.write_offset >= IMAGE_SIZE)
    {
        /** copy the color pallete to avoid overwritten */
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
}
