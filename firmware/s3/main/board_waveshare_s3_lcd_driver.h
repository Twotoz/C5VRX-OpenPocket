#pragma once

/*
 * Exact prototype target:
 *   Waveshare ESP32-S3-LCD-Driver-Board (SKU 27686)
 *   ESP32-S3-WROOM-1-N8R8
 *   40-pin 3SPI + RGB LCD connector
 *
 * Pin names below follow Waveshare's published schematic.  The runtime
 * C5<->S3 transport mapping is intentionally marked provisional because it
 * reuses pins that are only available after the RGB panel has been
 * initialized and therefore must be validated on real hardware.
 */

#define C5OP_BOARD_NAME "Waveshare ESP32-S3-LCD-Driver-Board 40PIN RGB"

/* 40-pin RGB panel control */
#define C5OP_LCD_INIT_SDA_GPIO   1
#define C5OP_LCD_INIT_SCK_GPIO   2
#define C5OP_LCD_INIT_CS_GPIO   42
#define C5OP_LCD_PCLK_GPIO      41
#define C5OP_LCD_DE_GPIO        40
#define C5OP_LCD_VSYNC_GPIO     39
#define C5OP_LCD_HSYNC_GPIO     38

/* RGB data: Waveshare leaves B0 and R0 unconnected. */
#define C5OP_LCD_B1_GPIO         5
#define C5OP_LCD_B2_GPIO        45
#define C5OP_LCD_B3_GPIO        48
#define C5OP_LCD_B4_GPIO        47
#define C5OP_LCD_B5_GPIO        21
#define C5OP_LCD_G0_GPIO        14
#define C5OP_LCD_G1_GPIO        13
#define C5OP_LCD_G2_GPIO        12
#define C5OP_LCD_G3_GPIO        11
#define C5OP_LCD_G4_GPIO        10
#define C5OP_LCD_G5_GPIO         9
#define C5OP_LCD_R1_GPIO        46
#define C5OP_LCD_R2_GPIO         3
#define C5OP_LCD_R3_GPIO         8
#define C5OP_LCD_R4_GPIO        18
#define C5OP_LCD_R5_GPIO        17

/* Shared board I2C / touch / TCA9554. */
#define C5OP_BOARD_I2C_SCL_GPIO  7
#define C5OP_BOARD_I2C_SDA_GPIO 15
#define C5OP_TOUCH_INT_GPIO      16

/* Native USB and the board's default UART header. */
#define C5OP_USB_DN_GPIO         19
#define C5OP_USB_DP_GPIO         20
#define C5OP_UART_TX_GPIO        43
#define C5OP_UART_RX_GPIO        44
#define C5OP_BAT_ADC_GPIO         4
#define C5OP_UNUSED_40PIN_GPIO    6

/*
 * Candidate full-QSPI pin plan for the exact 40-pin prototype.
 *
 * Preconditions:
 *  - initialize the RGB panel first over GPIO1/GPIO2 with LCD_CS (GPIO42),
 *    then keep LCD_CS high while GPIO1/GPIO2 are reused by SPI2;
 *  - hold the touch controller in reset before reusing GPIO16;
 *  - do not connect native USB while the video transport is active because
 *    GPIO19/GPIO20 become QSPI data lines;
 *  - benchmark this routing at the intended clock before treating 40 MHz as
 *    production-safe.
 *
 * CRSF remains on GPIO43/GPIO44.
 */
#define C5OP_QSPI_SCLK_GPIO       6
#define C5OP_QSPI_CS_GPIO        16
#define C5OP_QSPI_IO0_GPIO        1
#define C5OP_QSPI_IO1_GPIO        2
#define C5OP_QSPI_IO2_GPIO       19
#define C5OP_QSPI_IO3_GPIO       20

#define C5OP_CRSF_TX_GPIO        43
#define C5OP_CRSF_RX_GPIO        44
