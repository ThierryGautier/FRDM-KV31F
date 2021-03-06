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

/*! \file driver_MPL3115.c
    \brief Provides init() and read() functions for the MPL3115 pressure sensor/altimeter.
*/

#include "board.h"                      // generated by Kinetis Expert.  Long term - merge sensor_board.h into this file
#include "sensor_fusion.h"              // Sensor fusion structures and types
#include "sensor_io_i2c.h"              // Required for registerreadlist_t / registerwritelist_t declarations
#include "mpl3115_drv.h"                // Low level IS-SDK prototype driver
#include "drivers.h"                    // Device specific drivers supplied by NXP (can be replaced with user drivers)
#define MPL3115_MPERCOUNT 0.0000152587890625F		// Convert 32-bit Altitude value to meters
#define MPL3115_CPERCOUNT 0.00390625F			// Convert 16-bit Temperature value to Celcius
#define MPL3115_ACCEL_FIFO_SIZE 32

#if F_USING_PRESSURE  // driver is only pulled in if the appropriate flag is set in build.h

// Command definition to read the WHO_AM_I value.
const registerreadlist_t    MPL3115_WHO_AM_I_READ[] =
{
    { .readFrom = MPL3115_WHO_AM_I, .numBytes = 1 }, __END_READ_DATA__
};

// Command definition to read the number of entries in the accel FIFO.
const registerreadlist_t    MPL3115_F_STATUS_READ[] =
{
    { .readFrom = MPL3115_STATUS, .numBytes = 1 }, __END_READ_DATA__
};

// Command definition to read the number of entries in the accel FIFO.
registerreadlist_t          MPL3115_DATA_READ[] =
{
    { .readFrom = MPL3115_OUT_P_MSB, .numBytes = 5 }, __END_READ_DATA__
};

// Each entry in a RegisterWriteList is composed of: register address, value to write, bit-mask to apply to write (0 enables)
const registerwritelist_t   MPL3115_Initialization[] =
{
    // write 0000 0000 = 0x00 to MPL3115_CTRL_REG1 to place the MPL3115 in Standby
    // [7]: ALT=0
    // [6]: RAW=0
    // [5-3]: OS=000
    // [2]: RST=0
    // [1]: OST=0
    // [0]: SBYB=0 to enter standby
    { MPL3115_CTRL_REG1, 0x00, 0x00 },

    // write 1011 1001 = 0xB9 to configure MPL3115 and enter Active mode in Auto Acquisition mode and 1Hz ODR
    // [7]: ALT=1 for altitude measurements
    // [6]: RAW=0 to disable raw measurements
    // [5-3]: OS=111 for OS ratio=128
    // [2]: RST=0 do not enter reset
    // [1]: OST=0 do not initiate a reading
    // [0]: SBYB=1 to enter active mode
    { MPL3115_CTRL_REG1, 0xB9, 0x00 },
     __END_WRITE_DATA__
};

// All sensor drivers and initialization functions have the same prototype.
// sfg is a pointer to the master "global" sensor fusion structure.
// sensor = pointer to linked list element used by the sensor fusion subsystem to specify required sensors
int8_t MPL3115_Init(struct PhysicalSensor *sensor, SensorFusionGlobals *sfg)
{
    int32_t status;
    uint8_t reg;
    status = Register_I2C_Read(sensor->bus_driver, &sensor->deviceInfo, sensor->addr, MPL3115_WHO_AM_I, 1, &reg);
    if (status==SENSOR_ERROR_NONE) {
        sfg->Pressure.iWhoAmI = reg;
        if (reg!=MPL3115_WHOAMI_VALUE) {
             // return with error, whoAmI will retain default value of zero
            return(SENSOR_ERROR_INIT);
        }
    } else {
        // whoAmI will retain default value of zero
        return(status);
    }

    // Configure and start the MPL3115 sensor.  This does multiple register writes
    // (see MPL3115_Initialization definition above)
    status = Sensor_I2C_Write(sensor->bus_driver, &sensor->deviceInfo, sensor->addr, MPL3115_Initialization );

    // Stash some needed constants in the SF data structure for this sensor
    sfg->Pressure.fmPerCount = MPL3115_MPERCOUNT; // Convert 32-bit Altitude value to meters
    sfg->Pressure.fCPerCount = MPL3115_CPERCOUNT; // Convert 16-bit Temperature value to Celcius

    sensor->isInitialized = F_USING_PRESSURE | F_USING_TEMPERATURE;
    sfg->Pressure.isEnabled = true;

    return (status);
}

int8_t MPL3115_Read(struct PhysicalSensor *sensor, SensorFusionGlobals *sfg)
{
    uint8_t                     I2C_Buffer[6 * MPL3115_ACCEL_FIFO_SIZE];    // I2C read buffer
    int8_t                      status;         // I2C transaction status

    if(sensor->isInitialized != (F_USING_PRESSURE | F_USING_TEMPERATURE))
    {
        return SENSOR_ERROR_INIT;
    }

    status =  Sensor_I2C_Read(sensor->bus_driver, &sensor->deviceInfo, sensor->addr, MPL3115_DATA_READ, I2C_Buffer );

    if (status==SENSOR_ERROR_NONE) {
      	// place the read buffer into the 32 bit altitude and 16 bit temperature
	sfg->Pressure.iH = (I2C_Buffer[0] << 24) | (I2C_Buffer[1] << 16) | (I2C_Buffer[2] << 8);
	sfg->Pressure.iT = (I2C_Buffer[3] << 8) | I2C_Buffer[4];

	// convert from counts to metres altitude and C
	sfg->Pressure.fH = (float) sfg->Pressure.iH * sfg->Pressure.fmPerCount;
	sfg->Pressure.fT = (float) sfg->Pressure.iT * sfg->Pressure.fCPerCount;
    } else {
	sfg->Pressure.fH = 0.0;
	sfg->Pressure.fT = 0.0;
    }

    return (status);
}


// Each entry in a RegisterWriteList is composed of: register address, value to write, bit-mask to apply to write (0 enables)
const registerwritelist_t   MPL3115_IDLE[] =
{
  // Set ACTIVE = other bits unchanged
  { MPL3115_CTRL_REG1, 0x00, 0x01 },
    __END_WRITE_DATA__
};

// MPL3115_Idle places the sensor into Standby mode (wakeup time = 1 second)
int8_t MPL3115_Idle(struct PhysicalSensor *sensor, SensorFusionGlobals *sfg)
{
    int32_t     status;
    if(sensor->isInitialized == (F_USING_PRESSURE | F_USING_TEMPERATURE)) {
        status = Sensor_I2C_Write(sensor->bus_driver, &sensor->deviceInfo, sensor->addr, MPL3115_IDLE );
        sensor->isInitialized = 0;
        sfg->Pressure.isEnabled = false;
    } else {
      return SENSOR_ERROR_INIT;
    }
    return status;
}
#endif
