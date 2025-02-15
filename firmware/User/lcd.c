#include "lcd.h"

static void lcd_init_screen(const lcd_t* lcd);

void lcd_init(const lcd_t* lcd)
{
    /** Init peripherals */
    /** Init clock */
    uint32_t rcc_flag = RCC_APB2Periph_SPI1;
    if (lcd->clk.group == GPIOA ||
        lcd->mosi.group == GPIOA ||
        lcd->ncs.group == GPIOA ||
        lcd->nrst.group == GPIOA ||
        lcd->dc.group == GPIOA)
    {
        rcc_flag |= RCC_APB2Periph_GPIOA;
    }
    if (lcd->clk.group == GPIOB ||
        lcd->mosi.group == GPIOB ||
        lcd->ncs.group == GPIOB ||
        lcd->nrst.group == GPIOB ||
        lcd->dc.group == GPIOB)
    {
        rcc_flag |= RCC_APB2Periph_GPIOB;
    }
    if (lcd->clk.group == GPIOC ||
        lcd->mosi.group == GPIOC ||
        lcd->ncs.group == GPIOC ||
        lcd->nrst.group == GPIOC ||
        lcd->dc.group == GPIOC)
    {
        rcc_flag |= RCC_APB2Periph_GPIOC;
    }
    /** All gpio and spi are on APB2 */
    RCC_APB2PeriphClockCmd(rcc_flag, ENABLE);
    /** Init gpio */
    GPIO_InitTypeDef gpio_init = {0};
    /** NCS */
    gpio_init.GPIO_Pin = lcd->ncs.pin;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(lcd->ncs.group, &gpio_init);
    GPIO_WriteBit(lcd->ncs.group, lcd->ncs.pin, Bit_SET);
    /** DC */
    gpio_init.GPIO_Pin = lcd->dc.pin;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(lcd->dc.group, &gpio_init);
    GPIO_WriteBit(lcd->dc.group, lcd->dc.pin, Bit_SET);
    /** NRST */
    gpio_init.GPIO_Pin = lcd->nrst.pin;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(lcd->nrst.group, &gpio_init);
    GPIO_WriteBit(lcd->nrst.group, lcd->nrst.pin, Bit_SET);
    /** SPI SCK */
    gpio_init.GPIO_Pin = lcd->clk.pin;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(lcd->clk.group, &gpio_init);
    /** SPI MOSI */
    gpio_init.GPIO_Pin = lcd->mosi.pin;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(lcd->mosi.group, &gpio_init);
    /** SPI */
    SPI_InitTypeDef spi_init = {0};
    spi_init.SPI_Direction = SPI_Direction_1Line_Tx;
    spi_init.SPI_Mode = SPI_Mode_Master;
    spi_init.SPI_DataSize = SPI_DataSize_8b;
    spi_init.SPI_CPOL = SPI_CPOL_Low;
    spi_init.SPI_CPHA = SPI_CPHA_1Edge;
    spi_init.SPI_NSS = SPI_NSS_Soft;
    spi_init.SPI_BaudRatePrescaler = lcd->spi_prescaler;
    spi_init.SPI_FirstBit = SPI_FirstBit_MSB;
    spi_init.SPI_CRCPolynomial = 7;
    SPI_Init(lcd->spi, &spi_init);
    SPI_Cmd(lcd->spi, ENABLE);
    /** Init screen */
    lcd_init_screen(lcd);
}

static void inline __attribute__((always_inline)) gpio_set(const gpio_pin_t* pin)
{
    GPIO_WriteBit(pin->group, pin->pin, Bit_SET);
}

static void inline __attribute__((always_inline)) gpio_reset(const gpio_pin_t* pin)
{
    GPIO_WriteBit(pin->group, pin->pin, Bit_RESET);
}

/** This is slow. DO NOT use this for drawing */
static void inline __attribute__((always_inline)) send_command(
    const lcd_t* lcd, uint8_t command, uint8_t* data, int data_len)
{
    gpio_reset(&lcd->ncs);
    gpio_reset(&lcd->dc);
    SPI_I2S_SendData(lcd->spi, command);
    while (SPI_I2S_GetFlagStatus(lcd->spi, SPI_I2S_FLAG_BSY) == SET)
    {
    }
    gpio_set(&lcd->dc);
    for (int i = 0; i < data_len; i++)
    {
        SPI_I2S_SendData(lcd->spi, data[i]);
        while (SPI_I2S_GetFlagStatus(lcd->spi, SPI_I2S_FLAG_BSY) == SET)
        {
        }
    }
    gpio_set(&lcd->ncs);
}

static void lcd_init_screen(const lcd_t* lcd)
{
#define SEND_COMMAND(...) \
do \
{ \
    uint8_t data_2s7o4bi[] = {__VA_ARGS__}; \
    send_command(lcd, (data_2s7o4bi[0]), (data_2s7o4bi + 1), sizeof(data_2s7o4bi) - 1); \
} while(0)

    /** Reset */
    gpio_reset(&lcd->nrst);
    systick_delay_ms(10);
    gpio_set(&lcd->nrst);
    systick_delay_ms(100);

    /** Init */
    /** Sleep out */
    SEND_COMMAND(0x11);
    systick_delay_ms(120);
    /** Normal mode */
    SEND_COMMAND(0xB1, 0x05, 0x3C, 0x3C);
    /** Idle mode */
    SEND_COMMAND(0xB2, 0x05, 0x3C, 0x3C);
    /** Partial mode */
    SEND_COMMAND(0xB3, 0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C);
    /** Dot inversion */
    SEND_COMMAND(0xB4, 0x03);
    /** AVDD GVDD */
    SEND_COMMAND(0xC0, 0xAB, 0x0B, 0x04);
    /** VGH VGL */
    SEND_COMMAND(0xC1, 0xC5);
    /** Normal mode */
    SEND_COMMAND(0xC2, 0x0D, 0x00);
    /** Idle */
    SEND_COMMAND(0xC3, 0x8D, 0x6A);
    /** Partial + full */
    SEND_COMMAND(0xC4, 0x8D, 0xEE);
    /** VCOM */
    SEND_COMMAND(0xC5, 0x0F);
    /** Positive gamma */
    SEND_COMMAND(0xE0,
        0x07, 0x0E, 0x08, 0x07, 0x10, 0x07, 0x02, 0x07,
        0x09, 0x0F, 0x25, 0x36, 0x00, 0x08, 0x04, 0x10);
    /** Negative gamma */
    SEND_COMMAND(0xE1,
        0x0A, 0x0D, 0x08, 0x07, 0x0F, 0x07, 0x02, 0x07,
        0x09, 0x0F, 0x25, 0x35, 0x00, 0x09, 0x04, 0x10);
    SEND_COMMAND(0xFC, 0x80);
    SEND_COMMAND(0x3A, 0x05);
    /** Display direction, horizontal */
    SEND_COMMAND(0x36, 0xA8);
    /** Display inversion */
    SEND_COMMAND(0x21);
    /** Display on */
    SEND_COMMAND(0x29);
    /** Set column address, 1 -> 160 */
    SEND_COMMAND(0x2A, 0x00, 0x01, 0x00, 0xA0);
    /** Set row address, 26 -> 105 */
    SEND_COMMAND(0x2B, 0x00, 0x1A, 0x00, 0x69);

#undef SEND_COMMAND
}

void __attribute__((section(".ramcode"))) lcd_draw_image(
    const lcd_t* lcd,
    const uint8_t* indexes,
    const color_t* palette)
{
    gpio_reset(&lcd->ncs);
    gpio_reset(&lcd->dc);
    /** Set data size to 8b for the command */
    lcd->spi->CTLR1 &= (uint16_t)~SPI_DataSize_16b;
    lcd->spi->DATAR = 0x2C;
    /** Wait for transmission to fully complete */
    while (lcd->spi->STATR & SPI_I2S_FLAG_BSY)
    {
    }
    gpio_set(&lcd->dc);
    /** Set data size to 16b for the data */
    lcd->spi->CTLR1 |= SPI_DataSize_16b;
    
    /** Decode and display the color palette image */
    /** 
     * @todo improve the speed by packing the data on the server side into 32bit values.
     */
    uint8_t* p_index = (uint8_t*)indexes;
    uint8_t data = *p_index;
    int bits_left = 8;
    for (int i = 0; i < IMAGE_HEIGHT * IMAGE_WIDTH; i++)
    {
        int bits_read = 0;
        uint32_t index = 0;
        while (bits_read < PIXEL_BITS)
        {
            if (bits_left == 0)
            {
                bits_left = 8;
                p_index++;
                data = *p_index;
            }
            int bits_to_read = PIXEL_BITS - bits_read;
            if (bits_to_read > bits_left)
            {
                bits_to_read = bits_left;
            }
            index |= ((data >> (8 - bits_left)) & ((1 << bits_to_read) - 1)) << bits_read;
            bits_read += bits_to_read;
            bits_left -= bits_to_read;
        }

        color_t color = palette[index];
        while (!(lcd->spi->STATR & SPI_I2S_FLAG_TXE))
        {
        }
        lcd->spi->DATAR = color.raw;
    }

    /** Wait for transmission to fully complete */
    while (lcd->spi->STATR & SPI_I2S_FLAG_BSY)
    {
    }
}
