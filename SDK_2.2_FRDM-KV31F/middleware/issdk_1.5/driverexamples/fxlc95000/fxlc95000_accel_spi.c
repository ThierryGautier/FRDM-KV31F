/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
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
 * o Neither the name of the copyright holder nor the names of its
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
 * @file fxlc95000_accel_spi.c
 * @brief The fxlc95000_accel_spi.c file implements the ISSDK FXLC95000 sensor driver
 * example demonstration as for SPI Mode.
 */

//-----------------------------------------------------------------------
// SDK Includes
//-----------------------------------------------------------------------
#include "board.h"
#include "pin_mux.h"
#include "fsl_lptmr.h"
#include "clock_config.h"
#include "fsl_debug_console.h"

//-----------------------------------------------------------------------
// ISSDK Includes
//-----------------------------------------------------------------------
#include "issdk_hal.h"
#include "fxlc95000_drv.h"
#include "systick_utils.h"
#include "gpio_driver.h"

//-----------------------------------------------------------------------
// CMSIS Includes
//-----------------------------------------------------------------------
#include "Driver_SPI.h"
#include "Driver_GPIO.h"
#if (FSL_FEATURE_SOC_DSPI_COUNT > 0)
#include "fsl_dspi_cmsis.h"
#endif
#if (FSL_FEATURE_SOC_SPI_COUNT > 0)
#include "fsl_spi_cmsis.h"
#endif

//-----------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------
#define SAMPLING_RATE_us (100000)                /* Timeout for the ODR Timer. */
#define FXLC95000_SAMPLE_SIZE (10)               /* 4-Byte timestamp and 2-Byte X,Y,Z Data each. */
#define fxlc95000_odr_callback LPTMR0_IRQHandler /* Timer timeout Callback. */

//-----------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------
/*! Create commands for setting FXLC95000L desired configuration. */
const uint8_t cFxlc95000_SetODR_Cmd[] = {FXLC95000_SET_ODR_CMD_HDR, /* ODR equal to Sampling Rate. */
                                         FXLC95000_SST_ODR_PAYLOAD(SAMPLING_RATE_us)};
const uint8_t cFxlc95000_SetResolution_Cmd[] = {FXLC95000_SET_RESOLUTION_CMD_HDR, /* Resolution 14-bits. */
                                                FXLC95000_ACCEL_RESOLUTION_14_BIT};
const uint8_t cFxlc95000_SetRange_Cmd[] = {FXLC95000_SET_RANGE_CMD_HDR, /* FS Range 2G. */
                                           FXLC95000_ACCEL_RANGE_2G};

/*! Prepare the register write list to initialize FXLC95000L with desired MBox Settings. */
const registercommandlist_t cFxlc95000ConfigMBox[] = {
    {QuickReadInterruptDisable, 0, sizeof(QuickReadInterruptDisable)}, /* Disable QR INT. */
    {ConfigureMBoxCmd, 0, sizeof(ConfigureMBoxCmd)}, /* Configure MBox 16 to 25 with 10 byte Sample. */
    __END_WRITE_CMD__                                /* Ref. Table 3-7 of ISF1P195K_SW_REFERENCE_RM. */
};

/*! Prepare the register write list to configure FXLC95000L with desired Sampling Settings. */
const registercommandlist_t cFxlc95000ConfigSensor[] = {
    {StopDataCmd, 0, sizeof(StopDataCmd)},                                   /* Stop Data before (re)configuration. */
    {cFxlc95000_SetODR_Cmd, 0, sizeof(cFxlc95000_SetODR_Cmd)},               /* Set Sensor Sampling Rate. */
    {cFxlc95000_SetRange_Cmd, 0, sizeof(cFxlc95000_SetRange_Cmd)},           /* Set FS Range. */
    {cFxlc95000_SetResolution_Cmd, 0, sizeof(cFxlc95000_SetResolution_Cmd)}, /* Set Resolution */
    {StartDataCmd, 0, sizeof(StartDataCmd)},                                 /* Start Data after (re)configuration. */
    __END_WRITE_CMD__};

/*! Prepare the register read list to read the Timestamp and Accel data from FXLC95000. */
const registerreadlist_t cFxlc95000ReadSample[] = {
    {.readFrom = FXLC95000_SAMPLE_OFFSET, .numBytes = FXLC95000_SAMPLE_SIZE}, __END_READ_DATA__};

/*******************************************************************************
 * Globals
 ******************************************************************************/
volatile bool gFxlc95000DataRead;

/*******************************************************************************
 * Code
 ******************************************************************************/
/* LPTMR based ODR Callback function. */
void fxlc95000_odr_callback(void)
{
    LPTMR_ClearStatusFlags(LPTMR0, kLPTMR_TimerCompareFlag);
    gFxlc95000DataRead = true;
}

/*!
 * @brief Main function
 */
int main(void)
{
    int32_t status;
    lptmr_config_t lptmrConfig;
    fxlc95000_acceldata_t rawData;
    uint8_t data[FXLC95000_SAMPLE_SIZE];

    ARM_DRIVER_SPI *pSPIdriver = &SPI_S_DRIVER;
    fxlc95000_spi_sensorhandle_t fxlc95000Driver;

    /*! Initialize the MCU hardware. */
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_SystickEnable();
    BOARD_InitDebugConsole();

    /* Initialize ODR Timer. */
    LPTMR_GetDefaultConfig(&lptmrConfig);
    LPTMR_Init(LPTMR0, &lptmrConfig);
    LPTMR_EnableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
    LPTMR_SetTimerPeriod(LPTMR0, USEC_TO_COUNT(SAMPLING_RATE_us, CLOCK_GetFreq(kCLOCK_LpoClk)));
    EnableIRQ(LPTMR0_IRQn);

    PRINTF("\r\n ISSDK FXLC95000 sensor driver example for SPI Mode. \r\n");

    /*! Initialize the SPI driver. */
    status = pSPIdriver->Initialize(SPI_S_SIGNAL_EVENT);
    if (ARM_DRIVER_OK != status)
    {
        PRINTF("\r\n SPI Initialization Failed\r\n");
        return -1;
    }

    /*! Set the SPI Power mode. */
    status = pSPIdriver->PowerControl(ARM_POWER_FULL);
    if (ARM_DRIVER_OK != status)
    {
        PRINTF("\r\n SPI Power Mode setting Failed\r\n");
        return -1;
    }

    /*! Set the SPI Slave speed. */
    status = pSPIdriver->Control(ARM_SPI_MODE_MASTER | ARM_SPI_CPOL1_CPHA0, SPI_S_BAUDRATE);
    if (ARM_DRIVER_OK != status)
    {
        PRINTF("\r\n SPI Control Mode setting Failed\r\n");
        return -1;
    }

    /*! Initialize the FXLC95000 sensor driver. */
    status = FXLC95000_SPI_Initialize(&fxlc95000Driver, &SPI_S_DRIVER, SPI_S_DEVICE_INDEX, &FXLC95000_PDB_B,
                                      &FXLC95000_SSB_IO3, &FXLC95000_RST_GPIO, FXLC95000_BUILD_ID);
    if (SENSOR_ERROR_NONE != status)
    {
        PRINTF("\r\n Sensor Initialization Failed\r\n");
        return -1;
    }
    PRINTF("\r\n Successfully Initiliazed Sensor\r\n");

    /*!  Set the task to be executed while waiting for I2C transactions to complete. */
    FXLC95000_SPI_SetIdleTask(&fxlc95000Driver, (registeridlefunction_t)SMC_SetPowerModeWait, SMC);

    gFxlc95000DataRead = false; /* Do not read data, data will be read after ODR Timer expires. */

    /*! Configure the FXLC95000 with MBox settings. */
    status = FXLC95000_SPI_CommandResponse(&fxlc95000Driver, cFxlc95000ConfigMBox, NULL, NULL);
    if (SENSOR_ERROR_NONE != status)
    {
        PRINTF("\r\n FXLC95000 MBox Configuration Failed, Err = %d \r\n", status);
        return -1;
    }

    /*! Configure the FXLC95000 with Sampling settings. */
    status = FXLC95000_SPI_CommandResponse(&fxlc95000Driver, cFxlc95000ConfigSensor, NULL, NULL);
    if (SENSOR_ERROR_NONE != status)
    {
        PRINTF("\r\n FXLC95000 Sensor Configuration Failed, Err = %d \r\n", status);
        return -1;
    }
    PRINTF("\r\n Successfully Applied FXLC95000 Sensor Configuration\r\n");

    LPTMR_StartTimer(LPTMR0);
    for (;;) /* Forever loop */
    {
        if (gFxlc95000DataRead == false)
        {
            __NOP();
            continue;
        }
        else
        {
            gFxlc95000DataRead = false;
        }

        /*! Read the raw sensor data from the FXLC95000. */
        status = FXLC95000_SPI_CommandResponse(&fxlc95000Driver, NULL, cFxlc95000ReadSample, data);
        if (ARM_DRIVER_OK != status)
        {
            PRINTF("\r\n Read Failed. \r\n");
            return -1;
        }

        /*! Convert the raw bytes to sensor data with correct endianness. */
        rawData.timestamp = ((uint32_t)data[3] << 24) | ((uint32_t)data[2] << 16) | ((uint16_t)data[1] << 8) | data[0];
        rawData.accel[0] = ((int16_t)data[5] << 8) | data[4];
        rawData.accel[1] = ((int16_t)data[7] << 8) | data[6];
        rawData.accel[2] = ((int16_t)data[9] << 8) | data[8];

        PRINTF("\r\n Timestamp = 0x%X \r\n Accel X = %d  Y = %d  Z = %d \r\n", rawData.timestamp, rawData.accel[0],
               rawData.accel[1], rawData.accel[2]);
        ASK_USER_TO_RESUME(100); /* Ask for user input after processing 100 samples. */
    }
}
