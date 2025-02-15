#include <stdio.h>
#include "ch32x035_usbfs_device.h"

#define CDC_MIN_FLUSH_INTERVAL_US 1000

typedef struct
{
    uint8_t data[64];
    int size;
} cdc_tx_buffer_t;

static cdc_tx_buffer_t cdc_tx_buffer = 
{
    .size = 0,
};

static void cdc_flush()
{
    if (cdc_tx_buffer.size == 0)
    {
        return;
    }
    
    /** Avoid usb error */
    static uint64_t last_flush_ts = 0;
    uint64_t now = systick_get_time_us();
    if (now - last_flush_ts < CDC_MIN_FLUSH_INTERVAL_US)
    {
        systick_delay_us(CDC_MIN_FLUSH_INTERVAL_US - (now - last_flush_ts));
        last_flush_ts = systick_get_time_us();
    }
    else
    {
        last_flush_ts = now;
    }

    USBFS_Endp_DataUp(DEF_UEP3, cdc_tx_buffer.data, cdc_tx_buffer.size, DEF_UEP_CPY_LOAD);
    cdc_tx_buffer.size = 0;
}

static void inline __attribute__((__always_inline__)) cdc_putchar(int c)
{
    cdc_tx_buffer.data[cdc_tx_buffer.size] = c;
    cdc_tx_buffer.size++;
    if (cdc_tx_buffer.size == sizeof(cdc_tx_buffer.data))
    {
        cdc_flush();
    }
}

/*********************************************************************
 * @fn      _write
 *
 * @brief   Support Printf Function
 *
 * @param   *buf - UART send Data.
 *          size - Data length
 *
 * @return  size - Data length
 */
__attribute__((used))
int _write(int fd, char *buf, int size)
{
    for (int i = 0; i < size; i++)
    {
        cdc_putchar(buf[i]);
    }
    cdc_flush();
    return size;
}

/*********************************************************************
 * @fn      _sbrk
 *
 * @brief   Change the spatial position of data segment.
 *
 * @return  size - Data length
 */
__attribute__((used))
void *_sbrk(ptrdiff_t incr)
{
    extern char _end[];
    extern char _heap_end[];
    static char *curbrk = _end;

    if ((curbrk + incr < _end) || (curbrk + incr > _heap_end))
    return NULL - 1;

    curbrk += incr;
    return curbrk - incr;
}
