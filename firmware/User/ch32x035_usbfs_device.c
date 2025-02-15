/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32x035_usbfs_device.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/04/16
 * Description        : This file provides all the USBFS firmware functions.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "systick.h"
#include "ch32x035_usbfs_device.h"

/* Variable Definition */
/* Global */
const uint8_t *pUSBFS_Descr;

/* Setup Request */
volatile uint8_t USBFS_SetupReqCode;
volatile uint8_t USBFS_SetupReqType;
volatile uint16_t USBFS_SetupReqValue;
volatile uint16_t USBFS_SetupReqIndex;
volatile uint16_t USBFS_SetupReqLen;

/* USB Device Status */
volatile uint8_t USBFS_DevConfig;
volatile uint8_t USBFS_DevAddr;
volatile uint8_t USBFS_DevSleepStatus;
volatile uint8_t USBFS_DevEnumStatus;

/* Endpoint Buffer */
typedef struct
{
    __attribute__((aligned(4))) uint8_t EP0[DEF_USBD_UEP0_SIZE];
    __attribute__((aligned(4))) uint8_t EP1[DEF_USBD_ENDP1_SIZE];
    /** EP2 is for CDC data in (from host). Managed by application */
    /** EP3 buffer is not used. */
    // __attribute__((aligned(4))) uint8_t EP3[DEF_USBD_ENDP3_SIZE];
} usbfs_buffer_t;

static usbfs_buffer_t USBFS_Buffer;

#define pUSBFS_SetupReqPak            ((PUSB_SETUP_REQ)USBFS_Buffer.EP0)

/** UART config */
#define DEFAULT_UART_BAUDRATE         115200
#define DEFAULT_UART_STOPBIT          0
#define DEFAULT_UART_PARITY           0
#define DEFAULT_UART_DATABIT          8
#define DEFAULT_UART_RX_TIMEOUT       30

static uint8_t UART_CONFIG[] = {
    (uint8_t)( DEFAULT_UART_BAUDRATE ),
    (uint8_t)( DEFAULT_UART_BAUDRATE >> 8 ),
    (uint8_t)( DEFAULT_UART_BAUDRATE >> 16 ),
    (uint8_t)( DEFAULT_UART_BAUDRATE >> 24 ),
    DEFAULT_UART_STOPBIT,
    DEFAULT_UART_PARITY,
    DEFAULT_UART_DATABIT
};

/* USB IN Endpoint Busy Flag */
static volatile uint8_t USBFS_Endp_Busy[DEF_UEP_NUM];


/** Data hook */
extern uint8_t* cdc_hook_reset_rx_buffer();
extern uint8_t* cdc_hook_on_data(int len);

/*********************************************************************
 * @fn      USBFS_Device_Endp_Init
 *
 * @brief   Initializes USB device endpoints.
 *
 * @return  none
 */
static void USBFS_Device_Endp_Init(void)
{
    USBFSD->UEP4_1_MOD = USBFS_UEP1_TX_EN;
    USBFSD->UEP2_3_MOD = USBFS_UEP2_RX_EN | USBFS_UEP3_TX_EN;

    USBFSD->UEP0_DMA = (uint32_t)USBFS_Buffer.EP0;
    USBFSD->UEP1_DMA = (uint32_t)USBFS_Buffer.EP1;

    /** This is the initial RX DMA buffer */
    USBFSD->UEP2_DMA = (uint32_t)cdc_hook_reset_rx_buffer();

    /** This is not used. */
    USBFSD->UEP3_DMA = 0;

    USBFSD->UEP0_CTRL_H = USBFS_UEP_R_RES_ACK | USBFS_UEP_T_RES_NAK;
    USBFSD->UEP2_CTRL_H = USBFS_UEP_R_RES_ACK;

    USBFSD->UEP1_TX_LEN = 0;
    USBFSD->UEP3_TX_LEN = 0;

    USBFSD->UEP1_CTRL_H = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP3_CTRL_H = USBFS_UEP_T_RES_NAK;

    /* Clear End-points Busy Status */
    for (uint8_t i = 0; i < DEF_UEP_NUM; i++)
    {
        USBFS_Endp_Busy[i] = 0;
    }

}

/*********************************************************************
 * @fn      GPIO_USB_INIT
 *
 * @brief   Initializes USB GPIO.
 *
 * @return  none
 */
static void GPIO_USB_INIT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_16;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_17;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

/*********************************************************************
 * @fn      USBFS_Device_Init
 *
 * @brief   Initializes USB device.
 *
 * @return  none
 */
void USBFS_Device_Init(FunctionalState sta, PWR_VDD VDD_Voltage)
{
    if (sta)
    {
        GPIO_USB_INIT();
        if (VDD_Voltage == PWR_VDD_5V)
        {
            AFIO->CTLR = (AFIO->CTLR & ~(UDP_PUE_MASK | UDM_PUE_MASK | USB_PHY_V33)) | UDP_PUE_10K | USB_IOEN;
        }
        else
        {
            AFIO->CTLR = (AFIO->CTLR & ~(UDP_PUE_MASK | UDM_PUE_MASK)) | USB_PHY_V33 | UDP_PUE_1K5 | USB_IOEN;
        }
        USBFSD->BASE_CTRL = 0x00;
        USBFS_Device_Endp_Init();
        USBFSD->DEV_ADDR = 0x00;
        USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
        USBFSD->INT_FG = 0xff;
        USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
        USBFSD->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;
        NVIC_EnableIRQ(USBFS_IRQn);
    }
    else
    {
        AFIO->CTLR = AFIO->CTLR & ~(UDP_PUE_MASK | UDM_PUE_MASK | USB_IOEN);
        USBFSD->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
        systick_delay_us(10);
        USBFSD->BASE_CTRL = 0x00;
        NVIC_DisableIRQ(USBFS_IRQn);
    }
}

/*********************************************************************
 * @fn      USBFS_Endp_DataUp
 *
 * @brief   USBFS device data upload
 *
 * @return  none
 */
uint8_t USBFS_Endp_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len, uint8_t mod)
{
    uint8_t endp_mode;
    uint8_t buf_load_offset;
    uint16_t *uep_tx_len;
    uint16_t *uep_ctrl;
    uint8_t *uep_dma;

    switch (endp)
    {
    case 1:
        uep_tx_len = (uint16_t *)&USBFSD->UEP1_TX_LEN;
        uep_ctrl = (uint16_t *)&USBFSD->UEP1_CTRL_H;
        uep_dma = (uint8_t *)&USBFSD->UEP1_DMA;
        break;
    case 2:
        uep_tx_len = (uint16_t *)&USBFSD->UEP2_TX_LEN;
        uep_ctrl = (uint16_t *)&USBFSD->UEP2_CTRL_H;
        uep_dma = (uint8_t *)&USBFSD->UEP2_DMA;
        break;
    case 3:
        uep_tx_len = (uint16_t *)&USBFSD->UEP3_TX_LEN;
        uep_ctrl = (uint16_t *)&USBFSD->UEP3_CTRL_H;
        uep_dma = (uint8_t *)&USBFSD->UEP3_DMA;
        break;
    case 4:
        uep_tx_len = (uint16_t *)&USBFSD->UEP4_TX_LEN;
        uep_ctrl = (uint16_t *)&USBFSD->UEP4_CTRL_H;
        uep_dma = (uint8_t *)&USBFSD->UEP0_DMA;
        break;
    case 5:
        uep_tx_len = (uint16_t *)&USBFSD->UEP5_TX_LEN;
        uep_ctrl = (uint16_t *)&USBFSD->UEP5_CTRL_H;
        uep_dma = (uint8_t *)&USBFSD->UEP5_DMA;
        break;
    case 6:
        uep_tx_len = (uint16_t *)&USBFSD->UEP6_TX_LEN;
        uep_ctrl = (uint16_t *)&USBFSD->UEP6_CTRL_H;
        uep_dma = (uint8_t *)&USBFSD->UEP6_DMA;
        break;
    case 7:
        uep_tx_len = (uint16_t *)&USBFSD->UEP7_TX_LEN;
        uep_ctrl = (uint16_t *)&USBFSD->UEP7_CTRL_H;
        uep_dma = (uint8_t *)&USBFSD->UEP7_DMA;
        break;
    default:
        break;
    }

    /* DMA config, endp_ctrl config, endp_len config */
    if ((endp >= DEF_UEP1) && (endp <= DEF_UEP7))
    {
        if (USBFS_Endp_Busy[endp] == 0)
        {
            if ((endp == DEF_UEP1) || (endp == DEF_UEP4))
            {
                /* endp1/endp4 */
                endp_mode = USBFSD->UEP4_1_MOD;
                if (endp == DEF_UEP1)
                {
                    endp_mode = (uint8_t)(endp_mode >> 4);
                }
            }
            else if ((endp == DEF_UEP2) || (endp == DEF_UEP3))
            {
                /* endp2/endp3 */
                endp_mode = USBFSD->UEP2_3_MOD;
                if (endp == DEF_UEP3)
                {
                    endp_mode = (uint8_t)(endp_mode >> 4);
                }
            }
            else
            {
                /* endp5/endp6/endp7 */
                endp_mode = USBFSD->UEP567_MOD;
                if (endp == DEF_UEP5)
                {
                    endp_mode = (uint8_t)(endp_mode << 2);
                }
                else if (endp == DEF_UEP7)
                {
                    endp_mode = (uint8_t)(endp_mode >> 2);
                }

                endp_mode &= 0xfe;
            }

            if (endp_mode & USBFSD_UEP_TX_EN)
            {
                if (endp_mode & USBFSD_UEP_RX_EN)
                {
                    if (endp_mode & USBFSD_UEP_BUF_MOD)
                    {
                        if (*uep_ctrl & USBFS_UEP_T_TOG)
                        {
                            buf_load_offset = 192;
                        }
                        else
                        {
                            buf_load_offset = 128;
                        }
                    }
                    else
                    {
                        buf_load_offset = 64;
                    }
                }
                else
                {
                    if (endp_mode & USBFSD_UEP_BUF_MOD)
                    {
                        /* double tx buffer */
                        if (*uep_ctrl & USBFS_UEP_T_TOG)
                        {
                            buf_load_offset = 64;
                        }
                        else
                        {
                            buf_load_offset = 0;
                        }
                    }
                    else
                    {
                        buf_load_offset = 0;
                    }
                }

                if (endp == DEF_UEP4)
                {
                    buf_load_offset += 64;
                }

                if (buf_load_offset == 0)
                {
                    if (mod == DEF_UEP_DMA_LOAD)
                    {
                        /* DMA mode */
                        /** Why is there a uint16_t cast? */
                        /** This does not work at all. */
                        *uep_dma = (uint16_t)(uint32_t)pbuf;
                    }
                    else
                    {
                        /** 0x20000000 if the offset to ram. */
                        /* copy mode */
                        memcpy(((uint8_t *)(*((volatile uint32_t *)(uep_dma))) + 0x20000000), pbuf, len);
                    }
                }
                else
                {
                    memcpy(((uint8_t *)(*((volatile uint32_t *)(uep_dma))) + 0x20000000) + buf_load_offset, pbuf, len);
                }
                /* tx length */
                *uep_tx_len = len;
                /* response ack */
                *uep_ctrl = (*uep_ctrl & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;

                /* Set end-point busy */
                USBFS_Endp_Busy[endp] = 0x01;
            }
            else
            {
                return 1;
            }
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return 1;
    }
    return 0;
}

/*********************************************************************
 * @fn      USBFS_IRQHandler
 *
 * @brief   This function handles HD-FS exception.
 *
 * @return  none
 */
void __attribute__((interrupt("WCH-Interrupt-fast"))) __attribute__((section(".ramcode"))) USBFS_IRQHandler(void)
{

    uint8_t intflag, intst, errflag;
    uint16_t len;

    intflag = USBFSD->INT_FG;
    intst = USBFSD->INT_ST;

    if (intflag & USBFS_UIF_TRANSFER)
    {
        switch (intst & USBFS_UIS_TOKEN_MASK)
        {
        /* data-in stage processing */
        case USBFS_UIS_TOKEN_IN:
            switch (intst & USBFS_UIS_ENDP_MASK)
            {
            /* end-point 0 data in interrupt */
            case DEF_UEP0:
                if (USBFS_SetupReqLen == 0)
                {
                    USBFSD->UEP0_CTRL_H = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
                }
                if ((USBFS_SetupReqType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD)
                {
                    /* Non-standard request endpoint 0 Data upload */
                }
                else
                {
                    /* Standard request endpoint 0 Data upload */
                    switch (USBFS_SetupReqCode)
                    {
                    case USB_GET_DESCRIPTOR:
                        len = USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                        memcpy(USBFS_Buffer.EP0, pUSBFS_Descr, len);
                        USBFS_SetupReqLen -= len;
                        pUSBFS_Descr += len;
                        USBFSD->UEP0_TX_LEN = len;
                        USBFSD->UEP0_CTRL_H ^= USBFS_UEP_T_TOG;
                        break;

                    case USB_SET_ADDRESS:
                        USBFSD->DEV_ADDR = (USBFSD->DEV_ADDR & USBFS_UDA_GP_BIT) | USBFS_DevAddr;
                        break;

                    default:
                        break;
                    }
                }
                break;

            /* end-point 1 data in interrupt */
            case DEF_UEP1:
                USBFSD->UEP1_CTRL_H ^= USBFS_UEP_T_TOG;
                USBFSD->UEP1_CTRL_H = (USBFSD->UEP1_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                USBFS_Endp_Busy[DEF_UEP1] = 0;
                break;

            /* end-point 3 data in interrupt */
            case DEF_UEP3:
                USBFSD->UEP3_CTRL_H ^= USBFS_UEP_T_TOG;
                USBFSD->UEP3_CTRL_H = (USBFSD->UEP3_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                USBFS_Endp_Busy[DEF_UEP3] = 0;
                break;
                
            default:
                break;
            }
            break;

        /* data-out stage processing */
        case USBFS_UIS_TOKEN_OUT:
            switch (intst & USBFS_UIS_ENDP_MASK)
            {
            /* end-point 0 data out interrupt */
            case DEF_UEP0:
                len = USBFSD->RX_LEN;
                if (intst & USBFS_UIS_TOG_OK)
                {
                    if ((USBFS_SetupReqType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD)
                    {
                        /* Non-standard request end-point 0 Data download */
                        USBFS_SetupReqLen = 0;
                        if (USBFS_SetupReqCode == CDC_SET_LINE_CODING)
                        {
                            /** Save the uart config, though they are not used */
                            UART_CONFIG[ 0 ] = USBFS_Buffer.EP0[ 0 ];
                            UART_CONFIG[ 1 ] = USBFS_Buffer.EP0[ 1 ];
                            UART_CONFIG[ 2 ] = USBFS_Buffer.EP0[ 2 ];
                            UART_CONFIG[ 3 ] = USBFS_Buffer.EP0[ 3 ];
                            UART_CONFIG[ 4 ] = USBFS_Buffer.EP0[ 4 ];
                            UART_CONFIG[ 5 ] = USBFS_Buffer.EP0[ 5 ];
                            UART_CONFIG[ 6 ] = USBFS_Buffer.EP0[ 6 ];
                            /* restart usb receive  */
                            USBFSD->UEP2_DMA = (uint32_t)cdc_hook_reset_rx_buffer();
                            USBFSD->UEP2_CTRL_H &= ~USBFS_UEP_R_RES_MASK;
                            USBFSD->UEP2_CTRL_H |= USBFS_UEP_R_RES_ACK;
                        }
                    }
                    else
                    {
                        /* Standard request end-point 0 Data download */
                        /* Add your code here */
                    }
                    if (USBFS_SetupReqLen == 0)
                    {
                        USBFSD->UEP0_TX_LEN = 0;
                        USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
                    }
                }
                break;

            /* end-point 1 data out interrupt */
            case DEF_UEP2:
                USBFSD->UEP2_CTRL_H ^= USBFS_UEP_R_TOG;
                /** At this moment, the previous dma has finished */
                USBFSD->UEP2_DMA = (uint32_t)cdc_hook_on_data(USBFSD->RX_LEN);
                break;

            default:
                break;
            }
            break;

        /* Setup stage processing */
        case USBFS_UIS_TOKEN_SETUP:
            USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_NAK;

            /* Store All Setup Values */
            USBFS_SetupReqType = pUSBFS_SetupReqPak->bRequestType;
            USBFS_SetupReqCode = pUSBFS_SetupReqPak->bRequest;
            USBFS_SetupReqLen = pUSBFS_SetupReqPak->wLength;
            USBFS_SetupReqValue = pUSBFS_SetupReqPak->wValue;
            USBFS_SetupReqIndex = pUSBFS_SetupReqPak->wIndex;
            len = 0;
            errflag = 0;
            if ((USBFS_SetupReqType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD)
            {
                /* usb non-standard request processing */
                if (USBFS_SetupReqType & USB_REQ_TYP_CLASS)
                {
                    /* Class requests */
                    switch (USBFS_SetupReqCode)
                    {
                    case CDC_GET_LINE_CODING:
                        pUSBFS_Descr = UART_CONFIG;
                        len = sizeof(UART_CONFIG);
                        break;

                    case CDC_SET_LINE_CODING:
                        break;

                    case CDC_SET_LINE_CTLSTE:
                        break;

                    case CDC_SEND_BREAK:
                        break;

                    default:
                        errflag = 0xff;
                        break;
                    }
                }
                else if (USBFS_SetupReqType & USB_REQ_TYP_VENDOR)
                {
                    /* Manufacturer request */
                }
                else
                {
                    errflag = 0xFF;
                }

                /* Copy Descriptors to Endp0 DMA buffer */
                len = (USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                memcpy(USBFS_Buffer.EP0, pUSBFS_Descr, len);
                pUSBFS_Descr += len;
            }
            else
            {
                /* usb standard request processing */
                switch (USBFS_SetupReqCode)
                {
                /* get device/configuration/string/report/... descriptors */
                case USB_GET_DESCRIPTOR:
                    switch ((uint8_t)(USBFS_SetupReqValue >> 8))
                    {
                    /* get usb device descriptor */
                    case USB_DESCR_TYP_DEVICE:
                        pUSBFS_Descr = MyDevDescr;
                        len = DEF_USBD_DEVICE_DESC_LEN;
                        break;

                    /* get usb configuration descriptor */
                    case USB_DESCR_TYP_CONFIG:
                        pUSBFS_Descr = usb_desc_get_config_desc();
                        len = usb_desc_get_config_desc_len();
                        break;

                    /* get usb string descriptor */
                    case USB_DESCR_TYP_STRING:
                        switch ((uint8_t)(USBFS_SetupReqValue & 0xFF))
                        {
                        /* Descriptor 0, Language descriptor */
                        case DEF_STRING_DESC_LANG:
                            pUSBFS_Descr = MyLangDescr;
                            len = DEF_USBD_LANG_DESC_LEN;
                            break;

                        /* Descriptor 1, Manufacturers String descriptor */
                        case DEF_STRING_DESC_MANU:
                            pUSBFS_Descr = MyManuInfo;
                            len = DEF_USBD_MANU_DESC_LEN;
                            break;

                        /* Descriptor 2, Product String descriptor */
                        case DEF_STRING_DESC_PROD:
                            pUSBFS_Descr = MyProdInfo;
                            len = DEF_USBD_PROD_DESC_LEN;
                            break;

                        /* Descriptor 3, Serial-number String descriptor */
                        case DEF_STRING_DESC_SERN:
                            pUSBFS_Descr = MySerNumInfo;
                            len = DEF_USBD_SN_DESC_LEN;
                            break;

                        default:
                            errflag = 0xFF;
                            break;
                        }
                        break;

                    default:
                        errflag = 0xFF;
                        break;
                    }

                    /* Copy Descriptors to Endp0 DMA buffer */
                    if (USBFS_SetupReqLen > len)
                    {
                        USBFS_SetupReqLen = len;
                    }
                    len = (USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                    memcpy(USBFS_Buffer.EP0, pUSBFS_Descr, len);
                    pUSBFS_Descr += len;
                    break;

                /* Set usb address */
                case USB_SET_ADDRESS:
                    USBFS_DevAddr = (uint8_t)(USBFS_SetupReqValue & 0xFF);
                    break;

                /* Get usb configuration now set */
                case USB_GET_CONFIGURATION:
                    USBFS_Buffer.EP0[0] = USBFS_DevConfig;
                    if (USBFS_SetupReqLen > 1)
                    {
                        USBFS_SetupReqLen = 1;
                    }
                    break;

                /* Set usb configuration to use */
                case USB_SET_CONFIGURATION:
                    USBFS_DevConfig = (uint8_t)(USBFS_SetupReqValue & 0xFF);
                    USBFS_DevEnumStatus = 0x01;
                    break;

                /* Clear or disable one usb feature */
                case USB_CLEAR_FEATURE:
                    if ((USBFS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                    {
                        /* clear one device feature */
                        if ((uint8_t)(USBFS_SetupReqValue & 0xFF) == USB_REQ_FEAT_REMOTE_WAKEUP)
                        {
                            /* clear usb sleep status, device not prepare to sleep */
                            USBFS_DevSleepStatus &= ~0x01;
                        }
                    }
                    else if ((USBFS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                    {
                        /* Clear End-point Feature */
                        if ((uint8_t)(USBFS_SetupReqValue & 0xFF) == USB_REQ_FEAT_ENDP_HALT)
                        {
                            switch ((uint8_t)(USBFS_SetupReqIndex & 0xFF))
                            {
                            case (DEF_UEP_IN | DEF_UEP1):
                                /* Set End-point 1 IN NAK */
                                USBFSD->UEP1_CTRL_H = USBFS_UEP_T_RES_NAK;
                                break;

                            case (DEF_UEP_OUT | DEF_UEP2):
                                /* Set End-point 2 OUT ACK */
                                USBFSD->UEP2_CTRL_H = USBFS_UEP_R_RES_ACK;
                                break;

                            case (DEF_UEP_IN | DEF_UEP3):
                                /* Set End-point 3 IN NAK */
                                USBFSD->UEP3_CTRL_H = USBFS_UEP_T_RES_NAK;
                                break;

                            default:
                                errflag = 0xFF;
                                break;
                            }
                        }
                        else
                        {
                            errflag = 0xFF;
                        }
                    }
                    else
                    {
                        errflag = 0xFF;
                    }
                    break;

                /* set or enable one usb feature */
                case USB_SET_FEATURE:
                    if ((USBFS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                    {
                        /* Set Device Feature */
                        if ((uint8_t)(USBFS_SetupReqValue & 0xFF) == USB_REQ_FEAT_REMOTE_WAKEUP)
                        {
                            const uint8_t* MyCfgDescr = usb_desc_get_config_desc();
                            if (MyCfgDescr[7] & 0x20)
                            {
                                /* Set Wake-up flag, device prepare to sleep */
                                USBFS_DevSleepStatus |= 0x01;
                            }
                            else
                            {
                                errflag = 0xFF;
                            }
                        }
                        else
                        {
                            errflag = 0xFF;
                        }
                    }
                    else if ((USBFS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                    {
                        /* Set End-point Feature */
                        if ((uint8_t)(USBFS_SetupReqValue & 0xFF) == USB_REQ_FEAT_ENDP_HALT)
                        {
                            /* Set end-points status stall */
                            switch ((uint8_t)(USBFS_SetupReqIndex & 0xFF))
                            {
                            case (DEF_UEP_IN | DEF_UEP1):
                                /* Set End-point 1 IN STALL */
                                USBFSD->UEP1_CTRL_H = (USBFSD->UEP1_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
                                break;

                            case (DEF_UEP_OUT | DEF_UEP2):
                                /* Set End-point 2 OUT STALL */
                                USBFSD->UEP2_CTRL_H = (USBFSD->UEP2_CTRL_H & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL;
                                break;

                            case (DEF_UEP_IN | DEF_UEP3):
                                /* Set End-point 3 IN STALL */
                                USBFSD->UEP3_CTRL_H = (USBFSD->UEP3_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
                                break;

                            default:
                                errflag = 0xFF;
                                break;
                            }
                        }
                        else
                        {
                            errflag = 0xFF;
                        }
                    }
                    else
                    {
                        errflag = 0xFF;
                    }
                    break;

                /* This request allows the host to select another setting for the specified interface  */
                case USB_GET_INTERFACE:
                    USBFS_Buffer.EP0[0] = 0x00;
                    if (USBFS_SetupReqLen > 1)
                    {
                        USBFS_SetupReqLen = 1;
                    }
                    break;

                case USB_SET_INTERFACE:
                    break;

                /* host get status of specified device/interface/end-points */
                case USB_GET_STATUS:
                {
                    uint8_t* USBFS_EP0_Buf = USBFS_Buffer.EP0;
                    USBFS_EP0_Buf[ 0 ] = 0x00;
                    USBFS_EP0_Buf[ 1 ] = 0x00;
                    if ((USBFS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                    {
                        if (USBFS_DevSleepStatus & 0x01)
                        {
                            USBFS_EP0_Buf[0] = 0x02;
                        }
                    }
                    else if ((USBFS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                    {
                        switch ((uint8_t)(USBFS_SetupReqIndex & 0xFF))
                        {
                        case (DEF_UEP_IN | DEF_UEP1):
                            if (((USBFSD->UEP1_CTRL_H) & USBFS_UEP_T_RES_MASK) == USBFS_UEP_T_RES_STALL)
                            {
                                USBFS_EP0_Buf[0] = 0x01;
                            }
                            break;

                        case (DEF_UEP_OUT | DEF_UEP2):
                            if (((USBFSD->UEP2_CTRL_H) & USBFS_UEP_R_RES_MASK) == USBFS_UEP_R_RES_STALL)
                            {
                                USBFS_EP0_Buf[0] = 0x01;
                            }
                            break;

                        case (DEF_UEP_IN | DEF_UEP3):
                            if (((USBFSD->UEP3_CTRL_H) & USBFS_UEP_T_RES_MASK) == USBFS_UEP_T_RES_STALL)
                            {
                                USBFS_EP0_Buf[0] = 0x01;
                            }
                            break;

                        default:
                            errflag = 0xFF;
                            break;
                        }
                    }
                    else
                    {
                        errflag = 0xFF;
                    }

                    if (USBFS_SetupReqLen > 2)
                    {
                        USBFS_SetupReqLen = 2;
                    }

                } break;

                default:
                    errflag = 0xFF;
                    break;
                }
            }
            /* errflag = 0xFF means a request not support or some errors occurred, else correct */
            if (errflag == 0xff)
            {
                /* if one request not support, return stall */
                USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_STALL | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_STALL;
            }
            else
            {
                /* end-point 0 data Tx/Rx */
                if (USBFS_SetupReqType & DEF_UEP_IN)
                {
                    /* tx */
                    len = (USBFS_SetupReqLen > DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                    USBFS_SetupReqLen -= len;
                    USBFSD->UEP0_TX_LEN = len;
                    USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
                }
                else
                {
                    /* rx */
                    if (USBFS_SetupReqLen == 0)
                    {
                        USBFSD->UEP0_TX_LEN = 0;
                        USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
                    }
                    else
                    {
                        USBFSD->UEP0_CTRL_H = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
                    }
                }
            }
            break;

        /* Sof pack processing */
        case USBFS_UIS_TOKEN_SOF:
            break;

        default:
            break;
        }
        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    }
    else if (intflag & USBFS_UIF_BUS_RST)
    {
        /* usb reset interrupt processing */
        USBFS_DevConfig = 0;
        USBFS_DevAddr = 0;
        USBFS_DevSleepStatus = 0;
        USBFS_DevEnumStatus = 0;

        USBFSD->DEV_ADDR = 0;
        USBFS_Device_Endp_Init();
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
    }
    else if (intflag & USBFS_UIF_SUSPEND)
    {
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
        systick_delay_us(10);
        /* usb suspend interrupt processing */
        if (USBFSD->MIS_ST & USBFS_UMS_SUSPEND)
        {
            USBFS_DevSleepStatus |= 0x02;
            if (USBFS_DevSleepStatus == 0x03)
            {
                /* Handling usb sleep here */
            }
        }
        else
        {
            USBFS_DevSleepStatus &= ~0x02;
        }
    }
    else
    {
        /* other interrupts */
        USBFSD->INT_FG = intflag;
    }
}
