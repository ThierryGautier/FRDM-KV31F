/*
* Copyright (c) 2016, Freescale Semiconductor, Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* o Redistributions of source code must retain the above copyright notice, this list
*   of conditions and the following disclaimer.
*
* o Redistributions in binary form must reproduce the above copyright notice, this
*   list of conditions and the following disclaimer in the documentation and/or
*   other materials provided with the distribution.
*
* o Neither the name of Freescale Semiconductor, Inc. nor the names of its
*   contributors may be used to endorse or promote products derived from this
*   software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/** 
 * @file frdm_k31f.h
 * @brief The frdm_k31f.h file defines GPIO pin mappings for FRDM-K64F board
*/

#ifndef FRDM_KV31F_H_
#define FRDM_KV31F_H_

#include "pin_mux.h"
#include "fsl_i2c_cmsis.h"
#include "RTE_Device.h"
#include "Driver_GPIO.h"
#include "gpio_driver.h"

// I2C0 Handle
// PTD2 = I2C0_SCL
// PTD3 = I2C0_SDA
extern gpioHandleKSDK_t I2C0_SCL;
extern gpioHandleKSDK_t I2C0_SDA;

// I2C1 Handle
// PTC10 = I2C1_SCL
// PTC11 = I2C1_SDA
extern gpioHandleKSDK_t I2C1_SC;
extern gpioHandleKSDK_t I2C1_SDA;

// SPI0 Handle
// PTE17 = SPI0_SCK
// PTE18 = SPI0_SOUT
// PTE19 = SPI0_SIN
extern gpioHandleKSDK_t SPI0_SCK;
extern gpioHandleKSDK_t SPI0_SOUT;
extern gpioHandleKSDK_t SPI0_SIN;

// UART0 Handle
// PTB17 = UART0_TX
// PTB16 = UART0_RX
extern gpioHandleKSDK_t UART0_TX;
extern gpioHandleKSDK_t UART0_RX;

// UART0 Handle
// PTE0 = UART1_TX
// PTE1 = UART1_RX
extern gpioHandleKSDK_t UART1_TX;
extern gpioHandleKSDK_t UART1_RX;

// FRDM-KV31F Arduino Connector Pin Defintion
extern gpioHandleKSDK_t A0;
extern gpioHandleKSDK_t A1;
extern gpioHandleKSDK_t A2;
extern gpioHandleKSDK_t A3;
extern gpioHandleKSDK_t D0;
extern gpioHandleKSDK_t D1;
extern gpioHandleKSDK_t D2;
extern gpioHandleKSDK_t D3;
extern gpioHandleKSDK_t D4;
extern gpioHandleKSDK_t D5;
extern gpioHandleKSDK_t D6;
extern gpioHandleKSDK_t D7;
extern gpioHandleKSDK_t D8;
extern gpioHandleKSDK_t D9;
extern gpioHandleKSDK_t D10;

// FRDM-KV31F RGB LED Pin Definitions
extern gpioHandleKSDK_t RED_LED;
extern gpioHandleKSDK_t GREEN_LED;
extern gpioHandleKSDK_t BLUE_LED;

// I2C_S1: Pin mapping and driver information for default I2C brought to shield
#define I2C_S1_SCL_PIN I2C1_SCL
#define I2C_S1_SDA_PIN I2C1_SDA
#define I2C_S1_DRIVER Driver_I2C1

#define I2C_S1_DEVICE_INDEX I2C1_INDEX
#define I2C_S1_SIGNAL_EVENT I2C1_SignalEvent_t

// I2C_S2: Pin mapping and driver information for alternate I2C bus on shield
#define I2C_S2_SCL_PIN D15
#define I2C_S2_SDA_PIN D14
#define I2C_S2_DRIVER Driver_I2C0
#define I2C_S2_DEVICE_INDEX I2C0_INDEX
#define I2C_S2_SIGNAL_EVENT I2C0_SignalEvent_t

// I2C_B: Pin mapping and driver information for I2C routed on KV31F base board
#define I2C_BB_SCL_PIN I2C0_SCL
#define I2C_BB_SDA_PIN I2C0_SDA
#define I2C_BB_DRIVER Driver_I2C0
#define I2C_BB_DEVICE_INDEX I2C0_INDEX
#define I2C_BB_SIGNAL_EVENT I2C0_SignalEvent_t

// SPIS: Pin mapping and driver information default SPI brought to shield
#define SPI_S_SCLK D13
#define SPI_S_MOSI D11
#define SPI_S_MISO D12
#define SPI_S_DRIVER Driver_SPI0
#define SPI_S_BAUDRATE 500000U ///< Transfer baudrate - 500k
#define SPI_S_DEVICE_INDEX SPI0_INDEX
#define SPI_S_SIGNAL_EVENT SPI0_SignalEvent_t

// UART: Driver information for default UART to communicate with HOST PC.
#define HOST_S_DRIVER Driver_USART0
#define HOST_S_DEVICE_BASE UART0
#define HOST_S_CMSIS_HANDLE UART0_Handle
#define HOST_S_SIGNAL_EVENT HOST_SignalEvent_t

/* @brief  Ask use input to resume after specified samples have been processed. */
#define ASK_USER_TO_RESUME(x)                                                          \
    static volatile bool askResume = true;                                             \
    static uint16_t samplesToProcess = x - 1;                                          \
    if (askResume && !samplesToProcess--)                                              \
    {                                                                                  \
        PRINTF("\r\n Specified samples processed, press any key to continue... \r\n"); \
        GETCHAR();                                                                     \
        askResume = false;                                                             \
    }

/// @name Wired UART Parameters
/// Sensor Fusion aliases are defined in terms of specific hardware features
/// defined in MK22F51212.h.
///@{
#define WIRED_UART UART0                             ///< KSDK instance name for the debug UART
#define WIRED_UART_PORT_CLKEN kCLOCK_PortB           ///< KDSK handle for the pin port clock enable
#define WIRED_UART_PORT PORTB                        ///< KDSK handle for the pin port associated with this UART
#define WIRED_UART_RX_PIN 16U                         ///< The port number associated with RX
#define WIRED_UART_TX_PIN 17U                         ///< The port number associated with TX
#define WIRED_UART_MUX kPORT_MuxAlt3                 ///< KDSK pin mux selector
#ifndef USE_ORIENT_APP_CONTROL                       ///< If Using Orient App then use Host I/O
#define WIRED_UART_IRQHandler UART0_RX_TX_IRQHandler ///< KDSK-specified IRQ handler name
#endif
#define WIRED_UART_IRQn UART0_RX_TX_IRQn             ///< The interrupt number associated with this IRQ
#define WIRED_UART_CLKSRC UART0_CLK_SRC              ///< KSDK instance name for the clock feeding this module
#define WIRED_UART_IRQn UART0_RX_TX_IRQn             ///< KSDK interrupt vector number
///@}

/// @name Wireless UART Parameters
/// Sensor Fusion aliases are defined in terms of specific hardware features
/// defined in MK22F51212.h.
///@{
#define WIRELESS_UART UART1                             ///< KSDK instance name for the debug UART
#define WIRELESS_UART_PORT_CLKEN kCLOCK_PortE           ///< KDSK handle for the pin port clock enable
#define WIRELESS_UART_PORT PORTE                        ///< KDSK handle for the pin port associated with this UART
#define WIRELESS_UART_RX_PIN 1U                         ///< The port number associated with RX
#define WIRELESS_UART_TX_PIN 0U                         ///< The port number associated with TX
#define WIRELESS_UART_MUX kPORT_MuxAlt3                 ///< KDSK pin mux selector
#define WIRELESS_UART_IRQHandler UART1_RX_TX_IRQHandler ///< KDSK-specified IRQ handler name
#define WIRELESS_UART_IRQn UART1_RX_TX_IRQn             ///< The interrupt number associated with this IRQ
#define WIRELESS_UART_CLKSRC UART1_CLK_SRC              ///< KSDK instance name for the clock feeding this module
#define WIRELESS_UART_IRQn UART1_RX_TX_IRQn             ///< KSDK interrupt vector number
///@}

///@name Miscellaneous Hardware Configuration Parameters
///@{
#define THIS_BOARD FRDM_KV31F           ///< FRDM_KV31F
#define CORE_SYSTICK_HZ 120000000       ///< core and systick clock rate (Hz)
#define CALIBRATION_NVM_ADDR 0x0007F000 ///< start of final 4KB (sector size) of 512K flash
//#define ADS_NVM_ADDR 0x000FE000         ///< start of the next to last 1KB (sector size) of the 128KB flash
#define FLASH_SECTOR_SIZE_PROPERTY kFLASH_PropertyPflashSectorSize
#define FLASH_ERASE_KEY kFLASH_ApiEraseKey

// offsets from start of NVM block for calibration coefficients
#define MAG_NVM_OFFSET 0     // 68 bytes used
#define GYRO_NVM_OFFSET 100  // 16 bytes used
#define ACCEL_NVM_OFFSET 140 // 88 bytes used
///@}

// FXOS8700 Sensor Information of the mother board
#define MOTHER_BOARD_FXOS8700_I2C_ADDR 0x1D
#define FXOS8700_INT1 D2
#define FXOS8700_INT2 D4
#define FXOS8700_CS A2

#endif /* FRDM_KV31F_H_ */
