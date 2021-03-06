/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
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
 * @file orientaion_application_freertos_agm01.c
 * @brief The orientaion_application_freertos_agm01.c file implements the ISSDK
 *        freeRTOS orientation application for FRDM-STBC-AGM01 using sensor
 *        fusion core functional interfaces and host i/o interface.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "event_groups.h"

/* KSDK Headers */
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_port.h"
#include "fsl_i2c.h"

/* CMSIS Headers */
#include "Driver_USART.h"
#include "fsl_i2c_cmsis.h"
#include "fsl_uart_cmsis.h"

/* ISSDK Headers */
#include "fxas21002.h"
#include "fxos8700.h"
#include "register_io_i2c.h"
#include "host_io_uart.h"
#include "gpio_driver.h"
#include "auto_detection_service.h"

/* Sensor Fusion Headers */
#include "sensor_fusion.h"
#include "control.h"
#include "status.h"
#include "drivers.h"

/*******************************************************************************
 * Macro Definitions
 ******************************************************************************/
#define TIMESTAMP_DATA_SIZE (4) /* Orientation Packet: TimeStamp field size */
#define QUATERNION_SIZE (8)     /* Orientation Packet: Quaternion field size */
#define COORDINATES_SIZE (1)    /* Orientation Packet: coordinates field size */
#define BOARDINFO_SIZE (1)      /* Orientation Packet: BoardInfo field size */
#define BUILDNAME_SIZE (2)      /* Orientation Packet: BuildInfo field size */
#define SYSTICKINFO_SIZE (2)    /* Orientation Packet: SysTick field size */

/*! @brief Unique Name for this application which should match the target GUI pkg name. */
#define APPLICATION_NAME "ORIENTATION-FreeRTOS"
/*! @brief Version to distinguish between instances the same application based on target Shield and updates. */
#define APPLICATION_VERSION "1.0"

/* Orientation Streaming Packet Payload Size */
#define STREAMING_PAYLOAD_LEN \
    (TIMESTAMP_DATA_SIZE + QUATERNION_SIZE + COORDINATES_SIZE + BOARDINFO_SIZE + BUILDNAME_SIZE + SYSTICKINFO_SIZE)

/*******************************************************************************
 * Global Variables
 ******************************************************************************/
SensorFusionGlobals sfg;                       /* Sensor Fusion Global Structure Instance */
ControlSubsystem gOrientationControlSubsystem; /* Control Subsystem Structure Instance for orientation streaming */
StatusSubsystem statusSubsystem;               /* Sensor Fusion algorithm execution status indicators (LED) */
char boardString[ADS_MAX_STRING_LENGTH] = {0}, shieldString[ADS_MAX_STRING_LENGTH] = {0},
     embAppName[ADS_MAX_STRING_LENGTH] = {0};
volatile bool gStreamingEnabled; /* Variable indicating streaming mode. */
uint8_t gPrimaryStreamID;        /* The auto assigned Stream ID to be used to stream complete data. */

uint8_t orientOutputBuffer[STREAMING_HEADER_LEN + STREAMING_PAYLOAD_LEN];

struct PhysicalSensor sensors[2]; /* This implementation uses two physical sensors */
EventGroupHandle_t event_group = NULL;
registerDeviceInfo_t i2cBusInfo = {.deviceInstance = I2C_S_DEVICE_INDEX, .functionParam = NULL, .idleFunction = NULL};

/*******************************************************************************
 * Local Function Prototype Definitions
 ******************************************************************************/
/* FreeRTOS Task#1 definition */
static void read_task(void *pvParameters);
/* FreeRTOS Task#2 definition */
static void fusion_task(void *pvParameters);
/* Utility Function to add field bytes to orientation buffer */
static void updateOrientBuf(uint8_t *pDest, uint16_t *pIndex, uint8_t *pSrc, uint16_t iBytesToCopy);
/* Initialize the orientation application control subsystem */
static int8_t initOrientaionAppControlPort(ControlSubsystem *pComm);
/* Function to create orient packets for serial streaming over Host I/O */
static void encodeOrientPacketStream(SensorFusionGlobals *sfg, uint8_t *sUARTOutputBuffer);
/* Utility function for reading common algorithm parameters */
void readAlgoParams(
    SV_ptr data, Quaternion *fq, int16_t *iPhi, int16_t *iThe, int16_t *iRho, int16_t iOmega[], uint16_t *isystick);

/*******************************************************************************
 * Code
 ******************************************************************************/
/* Handler for Device Info and Streaming Control Commands. */
bool process_host_command(
    uint8_t tag, uint8_t *hostCommand, uint8_t *hostResponse, size_t *hostMsgSize, size_t respBufferSize)
{
    bool success = false;

    /* If it is Host requesting Device Info, send Board Name and Shield Name. */
    if (tag == HOST_PRO_INT_DEV_TAG)
    { /* Byte 1     : Payload - Length of APPLICATION_NAME (b)
       * Bytes=b    : Payload Application Name
       * Byte b+1   : Payload - Length of BOARD_NAME (s)
       * Bytes=s    : Payload Board Name
       * Byte b+s+2 : Payload - Length of SHIELD_NAME (v)
       * Bytes=v    : Payload Shield Name */

        size_t appNameLen = strlen(embAppName);
        size_t boardNameLen = strlen(boardString);
        size_t shieldNameLen = strlen(shieldString);

        if (respBufferSize >= boardNameLen + shieldNameLen + appNameLen + 3)
        { /* We have sufficient buffer. */
            *hostMsgSize = 0;
        }
        else
        {
            return false;
        }

        hostResponse[*hostMsgSize] = appNameLen;
        *hostMsgSize += 1;

        memcpy(hostResponse + *hostMsgSize, embAppName, appNameLen);
        *hostMsgSize += appNameLen;

        hostResponse[*hostMsgSize] = boardNameLen;
        *hostMsgSize += 1;

        memcpy(hostResponse + *hostMsgSize, boardString, boardNameLen);
        *hostMsgSize += boardNameLen;

        hostResponse[*hostMsgSize] = shieldNameLen;
        *hostMsgSize += 1;

        memcpy(hostResponse + *hostMsgSize, shieldString, shieldNameLen);
        *hostMsgSize += shieldNameLen;

        return true;
    }

    /* If it is Host sending Streaming Commands, take necessary actions. */
    if ((tag == (HOST_PRO_INT_CMD_TAG | HOST_PRO_CMD_W_CFG_TAG)) &&
        (*hostMsgSize == HOST_MSG_CMD_ACT_OFFSET - HOST_MSG_LEN_LSB_OFFSET))
    {                           /* Byte 1 : Payload - Operation Code (Start/Stop Operation Code)
                                 * Byte 2 : Payload - Stream ID (Target Stream for carrying out operation) */
        switch (hostCommand[0]) /* Execute desired operation (Start/Stop) on the requested Stream. */
        {
            case HOST_CMD_START:
                if (hostCommand[1] == gPrimaryStreamID && gStreamingEnabled == false)
                {
                    gStreamingEnabled = true;
                    success = true;
                }
                break;
            case HOST_CMD_STOP:
                if (hostCommand[1] == gPrimaryStreamID && gStreamingEnabled == true)
                {
                    gStreamingEnabled = false;
                    success = true;
                }
                break;
            default:
                break;
        }
        *hostMsgSize = 0; /* Zero payload in response. */
    }

    return success;
}

/*!
 * @brief freeRTOS read_task function
 */
static void read_task(void *pvParameters)
{
    uint16_t i = 0;
    portTickType lastWakeTime;
    const portTickType frequency = 1; /* tick counter runs at the read rate */
    lastWakeTime = xTaskGetTickCount();
    while (1)
    {
        for (i = 1; i <= OVERSAMPLE_RATE; i++)
        {
            vTaskDelayUntil(&lastWakeTime, frequency);
            /*! Reads sensors, applies HAL and does averaging (if applicable) */
            sfg.readSensors(&sfg, i);
        }
        xEventGroupSetBits(event_group, B0);
    }
}

/*!
 * @brief freeRTOS fusion_task function
 */
static void fusion_task(void *pvParameters)
{
    uint16_t i = 0;
    while (1)
    {
        xEventGroupWaitBits(event_group,    /* The event group handle. */
                            B0,             /* The bit pattern the event group is waiting for. */
                            pdTRUE,         /* BIT_0 and BIT_4 will be cleared automatically. */
                            pdFALSE,        /* Don't wait for both bits, either bit unblock task. */
                            portMAX_DELAY); /* Block indefinitely to wait for the condition to be met. */

        sfg.conditionSensorReadings(&sfg); /* magCal is run as part of this */
        sfg.runFusion(&sfg);               /* Run the actual fusion algorithms */
        sfg.applyPerturbation(&sfg);       /* apply debug perturbation (testing only) */

        sfg.loopcounter++; /* The loop counter is used to "serialize" mag cal operations */
        i = i + 1;
        if (i >= 4)
        {                           /* Some status codes include a "blink" feature.  This loop */
            i = 0;                  /* should cycle at least four times for that to operate correctly */
            sfg.updateStatus(&sfg); /* This is where pending status updates are made visible */
        }
        sfg.queueStatus(&sfg, NORMAL); /* assume NORMAL status for next pass through the loop */

        /* Check for incoming commands form Host. */
        Host_IO_Receive(process_host_command, HOST_FORMAT_HDLC);

        sfg.pControlSubsystem->stream(&sfg,
                                      orientOutputBuffer); /*! Encode Orietantion Stream Packet and send over Host I/O*/
    }
}

/*!
 * @brief updateOrientBuf utility function
 */
void updateOrientBuf(uint8_t *pDest, uint16_t *pIndex, uint8_t *pSrc, uint16_t iBytesToCopy)
{
    uint16_t counter;

    for (counter = 0; counter < iBytesToCopy; counter++)
    {
        pDest[(*pIndex)++] = pSrc[counter];
    }

    return;
}

/*!
 * @brief Utility function for reading common algorithm parameters
 */
void readAlgoParams(
    SV_ptr data, Quaternion *fq, int16_t *iPhi, int16_t *iThe, int16_t *iRho, int16_t iOmega[], uint16_t *isystick)
{
    *fq = data->fq;
    iOmega[CHX] = (int16_t)(data->fOmega[CHX] * 20.0F);
    iOmega[CHY] = (int16_t)(data->fOmega[CHY] * 20.0F);
    iOmega[CHZ] = (int16_t)(data->fOmega[CHZ] * 20.0F);
    *iPhi = (int16_t)(10.0F * data->fPhi);
    *iThe = (int16_t)(10.0F * data->fThe);
    *iRho = (int16_t)(10.0F * data->fRho);
    *isystick = (uint16_t)(data->systick / 20);
}

/*!
 * @brief Initialize the orientation application control subsystem
 */
static int8_t initOrientaionAppControlPort(ControlSubsystem *pComm /* pointer to the control subystem structure */
                                           )
{
    if (pComm)
    {
        pComm->DefaultQuaternionPacketType = Q3; /* default to simplest algorithm */
        pComm->QuaternionPacketType = Q3;        /* default to simplest algorithm */
        pComm->AngularVelocityPacketOn = true;   /* transmit angular velocity packet */
        pComm->DebugPacketOn = true;             /* transmit debug packet */
        pComm->RPCPacketOn = true;               /* transmit roll, pitch, compass packet */
        pComm->AltPacketOn = true;               /* Altitude packet */
        pComm->AccelCalPacketOn = 0;
        pComm->write = NULL;
        pComm->stream = encodeOrientPacketStream; /* Function to create orient packets for serial stream */
        memset(orientOutputBuffer, 0, sizeof(orientOutputBuffer));
        return (0);
    }
    else
    {
        return (1);
    }
}

/*!
 * @brief Function to create orient packets for serial streaming over Host I/O
 */
void encodeOrientPacketStream(SensorFusionGlobals *sfg, uint8_t *sUARTOutputBuffer)
{
    Quaternion fq;                  /* quaternion to be transmitted */
    static uint32_t iTimeStamp = 0; /* 1MHz time stamp */
    uint16_t iIndex;                /* output buffer counter */
    int16_t scratch16;              /* scratch int16_t */
    int16_t iPhi, iThe, iRho;       /* integer angles to be transmitted */
    int16_t iDelta;                 /* magnetic inclination angle if available */
    int16_t iOmega[3];              /* scaled angular velocity vector */
    uint16_t isystick;              /* algorithm systick time */
    uint8_t tmpuint8_t;             /* scratch uint8_t */
    uint8_t flags;                  /* byte of flags */
    size_t sUARTOutputBufLen = STREAMING_PAYLOAD_LEN;

    /* Update the 1 microsecond timestamp counter */
    iTimeStamp += 1000000 / FUSION_HZ;

    /* cache local copies of control flags so we don't have to keep dereferencing pointers below */
    quaternion_type quaternionPacketType;
    quaternionPacketType = sfg->pControlSubsystem->QuaternionPacketType;

    /* Start the index to store orientation packet payload into the transmit buffer */
    iIndex = STREAMING_HEADER_LEN;

    /*! Add 1 microsecond timestamp counter (4 bytes) value to Orientation Buffer */
    updateOrientBuf(sUARTOutputBuffer, &iIndex, (uint8_t *)&iTimeStamp, 4);

    /* initialize default quaternion, flags byte, angular velocity and orientation */
    fq.q0 = 1.0F;
    fq.q1 = fq.q2 = fq.q3 = 0.0F;
    flags = 0x00;
    iOmega[CHX] = iOmega[CHY] = iOmega[CHZ] = 0;
    iPhi = iThe = iRho = iDelta = 0;
    isystick = 0;

    /* set the quaternion, flags, angular velocity and Euler angles */
    if (sfg->iFlags & F_9DOF_GBY_KALMAN)
    {
        flags |= 0x08;
        iDelta = (int16_t)(10.0F * sfg->SV_9DOF_GBY_KALMAN.fDeltaPl);
        readAlgoParams((SV_ptr)&sfg->SV_9DOF_GBY_KALMAN, &fq, &iPhi, &iThe, &iRho, iOmega, &isystick);
    }

    /*! Scale the quaternion and add to the Orientation Buffer  */
    scratch16 = (int16_t)(fq.q0 * 30000.0F);
    updateOrientBuf(sUARTOutputBuffer, &iIndex, (uint8_t *)&scratch16, 2);
    scratch16 = (int16_t)(fq.q1 * 30000.0F);
    updateOrientBuf(sUARTOutputBuffer, &iIndex, (uint8_t *)&scratch16, 2);
    scratch16 = (int16_t)(fq.q2 * 30000.0F);
    updateOrientBuf(sUARTOutputBuffer, &iIndex, (uint8_t *)&scratch16, 2);
    scratch16 = (int16_t)(fq.q3 * 30000.0F);
    updateOrientBuf(sUARTOutputBuffer, &iIndex, (uint8_t *)&scratch16, 2);

/* Set the coordinate system bits in flags from default NED (00) */
#if THISCOORDSYSTEM == ANDROID
    /* Set the Android flag bits */
    flags |= 0x10;
#elif THISCOORDSYSTEM == WIN8
    /* set the Win8 flag bits */
    flags |= 0x20;
#endif // THISCOORDSYSTEM

    /*! Add the co-ordinate info byte to the Orientation Buffer */
    updateOrientBuf(sUARTOutputBuffer, &iIndex, &flags, 1);

    /*! Add the shield (bits 7-5) and Kinetis (bits 4-0) byte to the Orientation Buffer */
    tmpuint8_t = ((THIS_SHIELD & 0x07) << 5) | (THIS_BOARD & 0x1F);
    updateOrientBuf(sUARTOutputBuffer, &iIndex, &tmpuint8_t, 1);

    /*! Add Software version number to the Orientation Buffer */
    scratch16 = THISBUILD;
    updateOrientBuf(sUARTOutputBuffer, &iIndex, (uint8_t *)&scratch16, 2);

    /*! Add systick count to the Orientation Buffer */
    updateOrientBuf(sUARTOutputBuffer, &iIndex, (uint8_t *)&isystick, 2);

    /*! Send packet to host only if streaming has been enabled by Host. */
    if (gStreamingEnabled)
    {
        /*! Add Host I/O ISO streaming header */
        Host_IO_Add_ISO_Header(gPrimaryStreamID, sUARTOutputBuffer, sUARTOutputBufLen);

        /*! Send Orientation streaming packet to Host using Host I/O */
        Host_IO_Send(sUARTOutputBuffer, STREAMING_HEADER_LEN + sUARTOutputBufLen, HOST_FORMAT_HDLC);
    }

    return;
}

/*!
 * @brief Main function
 */
int main(void)
{
    int32_t status;
    uint8_t secondaryStreamID;              /* The auto assigned Stream ID not to be used to stream data. */
    ARM_DRIVER_I2C *I2Cdrv = &I2C_S_DRIVER; /* defined in the <shield>.h file */
    ARM_DRIVER_USART *pUartDriver = &HOST_S_DRIVER;

    /*! Initialize the MCU hardware. */
    BOARD_BootClockRUN();
    BOARD_SystickEnable();

    /* Create the Short Application Name String for ADS. */
    sprintf(embAppName, "%s:%s", APPLICATION_NAME, APPLICATION_VERSION);

    /* Run ADS. */
    BOARD_RunADS(embAppName, boardString, shieldString, ADS_MAX_STRING_LENGTH);

    /* Create the Full Application Name String for Device Info Response. */
    sprintf(embAppName, "%s:%s:%s", SHIELD_NAME, APPLICATION_NAME, APPLICATION_VERSION);

    /*! Initialize and set the KSDK driver for the I2C port */
    I2Cdrv->Initialize(I2C_S_SIGNAL_EVENT);
    I2Cdrv->PowerControl(ARM_POWER_FULL);
    I2Cdrv->Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_FAST);

    /*! Initialize the UART driver. */
    status = pUartDriver->Initialize(HOST_S_SIGNAL_EVENT);
    if (ARM_DRIVER_OK != status)
    {
        return -1;
    }
    /*! Set UART Power mode. */
    status = pUartDriver->PowerControl(ARM_POWER_FULL);
    if (ARM_DRIVER_OK != status)
    {
        return -1;
    }
    /*! Set UART Baud Rate. */
    status = pUartDriver->Control(ARM_USART_MODE_ASYNCHRONOUS, BOARD_DEBUG_UART_BAUDRATE);
    if (ARM_DRIVER_OK != status)
    {
        return -1;
    }

    /*! Initialize control sub-system for orientation packet streaming */
    initOrientaionAppControlPort(&gOrientationControlSubsystem);

    /*! Initialize sensor fusion status sub-system */
    initializeStatusSubsystem(&statusSubsystem);
    /*! Initialize sensor fusion global metadata */
    initSensorFusionGlobals(&sfg, &statusSubsystem, &gOrientationControlSubsystem);

    /*! Install the sensors to be used by sensor fusion */
    sfg.installSensor(&sfg, &sensors[0], FXOS8700_I2C_ADDR, 1, (void *)I2Cdrv, &i2cBusInfo, FXOS8700_Init,
                      FXOS8700_Read);
    sfg.installSensor(&sfg, &sensors[1], FXAS21002_I2C_ADDR, 1, (void *)I2Cdrv, &i2cBusInfo, FXAS21002_Init,
                      FXAS21002_Read);

    /*! Initialize streaming and assign Stream IDs. */
    gStreamingEnabled = false;
    /* Registering the FXOS8700 slave address with Host I/O for Register Read/Write. */
    gPrimaryStreamID =
        Host_IO_Init(pUartDriver, (void *)sensors[0].bus_driver, &sensors[0].deviceInfo, NULL, FXOS8700_I2C_ADDR);
    /* Confirm if a valid Stream ID has been allocated for this stream. */
    if (0 == gPrimaryStreamID)
    {
        return -1;
    }

    /* Registering the FXAS21002 slave address with Host I/O for Register Read/Write. */
    secondaryStreamID =
        Host_IO_Init(pUartDriver, (void *)sensors[1].bus_driver, &sensors[1].deviceInfo, NULL, FXAS21002_I2C_ADDR);
    /* Confirm if a valid Stream ID has been allocated for this stream. */
    if (0 == secondaryStreamID)
    {
        return -1;
    }

    /*! Initialize sensor fusion engine */
    sfg.initializeFusionEngine(&sfg);

    /*! Create RTOS tasks for sensor fusion */
    event_group = xEventGroupCreate();
    xTaskCreate(read_task, "READ", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(fusion_task, "FUSION", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

    /*! Set sensor fusion engine status to NORMAL */
    sfg.setStatus(&sfg, NORMAL);

    /*! Start RTOS Scheduler */
    vTaskStartScheduler();

    sfg.setStatus(&sfg, HARD_FAULT); /* If we got this far, FreeRTOS does not have enough memory allocated */
    for (;;)
        ;
}

/// \endcode
