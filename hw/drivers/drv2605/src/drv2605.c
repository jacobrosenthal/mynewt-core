/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>
#include <errno.h>
#include <assert.h>

#include "defs/error.h"
#include "os/os.h"
#include "sysinit/sysinit.h"
#include "hal/hal_i2c.h"
#include "drv2605/drv2605.h"
#include "drv2605_priv.h"

#if MYNEWT_VAL(DRV2605_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(DRV2605_STATS)
#include "stats/stats.h"
#endif

#if MYNEWT_VAL(DRV2605_STATS)
/* Define the stats section and records */
STATS_SECT_START(drv2605_stat_section)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(drv2605_stat_section)
    STATS_NAME(drv2605_stat_section, errors)
STATS_NAME_END(drv2605_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(drv2605_stat_section) g_drv2605stats;
#endif

#if MYNEWT_VAL(DRV2605_LOG)
#define LOG_MODULE_DRV2605 (306)
#define DRV2605_INFO(...)  LOG_INFO(&_log, LOG_MODULE_DRV2605, __VA_ARGS__)
#define DRV2605_ERR(...)   LOG_ERROR(&_log, LOG_MODULE_DRV2605, __VA_ARGS__)
static struct log _log;
#else
#define DRV2605_INFO(...)
#define DRV2605_ERR(...)
#endif


/**
 * Writes a single byte to the specified register
 *
 * @param The Sesnsor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC, 1);
    if (rc) {
        DRV2605_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                       data_struct.address, reg, value);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
    }

    return rc;
}

/**
 * Writes a multiple bytes to the specified register
 *
 * @param The Sesnsor interface
 * @param The register address to write to
 * @param The data buffer to write from
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_writelen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                      uint8_t len)
{
    int rc;
    uint8_t payload[20] = { reg, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0,
                               0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = len + 1,
        .buffer = payload
    };

    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        DRV2605_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
        STATS_INC(g_drv2605stats, errors);
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Reads a single byte from the specified register
 *
 * @param The Sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;
    uint8_t payload;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &payload
    };

    /* Register write */
    payload = reg;
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 0);
    if (rc) {
        DRV2605_ERR("I2C register write failed at address 0x%02X:0x%02X\n",
                   data_struct.address, reg);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
        goto err;
    }

    /* Read one byte back */
    payload = 0;
    rc = hal_i2c_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    *value = payload;
    if (rc) {
        DRV2605_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, reg);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
    }

err:
    return rc;
}

/**
 * Read data from the sensor of variable length (MAX: 8 bytes)
 *
 * @param The Sensor interface
 * @param Register to read from
 * @param Bufer to read into
 * @param Length of the buffer
 *
 * @return 0 on success and non-zero on failure
 */
int
drv2605_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
               uint8_t len)
{
    int rc;
    uint8_t payload[23] = { reg, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    /* Clear the supplied buffer */
    memset(buffer, 0, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 0);
    if (rc) {
        DRV2605_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
        goto err;
    }

    /* Read len bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = hal_i2c_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        DRV2605_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, reg);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
        goto err;
    }

    /* Copy the I2C results into the supplied buffer */
    memcpy(buffer, payload, len);

    return 0;
err:
    return rc;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this accellerometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_init(struct os_dev *dev, void *arg)
{
    struct drv2605 *drv2605;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    drv2605 = (struct drv2605 *) dev;

#if MYNEWT_VAL(DRV2605_LOG)
    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif

    sensor = &drv2605->sensor;

#if MYNEWT_VAL(DRV2605_STATS)
    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_drv2605stats),
        STATS_SIZE_INIT_PARMS(g_drv2605stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(drv2605_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_drv2605stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto err;
    }

    //TODO.. since im not a sensor my config isnt being called. I dont seem to need to wait to talk to device so im just going to setup here
    rc = drv2605_config(drv2605);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Get chip ID from the sensor
 *
 * @param The sensor interface
 * @param Pointer to the variable to fill up chip ID in
 * @return 0 on success, non-zero on failure
 */
int
drv2605_get_chip_id(struct sensor_itf *itf, uint8_t *id)
{
    int rc;
    uint8_t idtmp;

    /* Check if we can read the chip address */
    rc = drv2605_read8(itf, DRV2605_STATUS_ADDR, &idtmp);
    if (rc) {
        goto err;
    }

    *id = (idtmp & DRV2605_STATUS_DEVICE_ID_MASK) >> DRV2605_STATUS_DEVICE_ID_POS;

    return 0;
err:
    return rc;
}

int
drv2605_reset(struct sensor_itf *itf)
{
    int rc;
    uint8_t temp;
    uint8_t interval = 255;

    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_RESET);
    if (rc) {
        goto err;
    }

    //When reset is complete, the reset bit automatically clears. Timeout after 255 x 5ms or 1275ms
    do{
        os_time_delay((OS_TICKS_PER_SEC * 5)/1000 + 1);
        rc = drv2605_read8(itf, DRV2605_MODE_ADDR, &temp);
    } while (!rc && interval-- && (temp & DRV2605_MODE_RESET));

    //if we timed out
    if (!interval) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    return 0;
err:
    return rc;
}


//NOTE diagnistics (and frankly all operation) will in all likelyhood fail if your motor is not SECURED to a mass
//it cant be floating on your desk even for prototyping
//if successful restores mode bit
int
drv2605_diagnostics(struct sensor_itf *itf)
{
    int rc;
    uint8_t temp;
    uint8_t interval = 255;
    uint8_t last_mode;

    rc = drv2605_read8(itf, DRV2605_MODE_ADDR, &last_mode);
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_DIAGNOSTICS);
    if (rc) {
        goto err;
    }

    //4. Set the GO bit (write 0x01 to register 0x0C) to start the auto-calibration process
    rc = drv2605_write8(itf, DRV2605_GO_ADDR, DRV2605_GO_GO);
    if (rc) {
        goto err;
    }

    //When diagnostic is complete, the GO bit automatically clears. Timeout after 255 x 5ms or 1275ms
    do{
        os_time_delay((OS_TICKS_PER_SEC * 5)/1000 + 1);
        rc = drv2605_read8(itf, DRV2605_GO_ADDR, &temp);
    } while (!rc && interval-- && (temp & DRV2605_GO_GO));

    //if we timed out
    if (!interval) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    //5. Check the status of the DIAG_RESULT bit (in register 0x00) to ensure that the diagnostic routine is complete without faults.
    rc = drv2605_read8(itf, DRV2605_STATUS_ADDR, &temp);
    if (rc || (temp & DRV2605_STATUS_DIAG_RESULT_FAIL)) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    //return to mode at start
    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, last_mode | DRV2605_MODE_MODE_POS);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}


int
drv2605_send_defaults(struct sensor_itf *itf)
{
    int rc;

    rc = drv2605_write8(itf, DRV2605_RATED_VOLTAGE_ADDR, MYNEWT_VAL(DRV2605_RATED_VOLTAGE));
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_OVERDRIVE_CLAMP_VOLTAGE_ADDR, MYNEWT_VAL(DRV2605_OD_CLAMP));
    if (rc) {
        goto err;
    }

    //todo LRA specific
    rc = drv2605_write8(itf, DRV2605_FEEDBACK_CONTROL_ADDR, ((MYNEWT_VAL(DRV2605_CALIBRATED_BEMF_GAIN) & DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_MAX) << DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_POS) | DRV2605_FEEDBACK_CONTROL_N_LRA );
    if (rc) {
        goto err;
    }

    // They seem to always enable startup boost in the dev kit so throw it in?
    rc = drv2605_write8(itf, DRV2605_CONTROL1_ADDR, ((MYNEWT_VAL(DRV2605_DRIVE_TIME) & DRV2605_CONTROL1_DRIVE_TIME_MAX) << DRV2605_CONTROL1_DRIVE_TIME_POS) | DRV2605_CONTROL1_STARTUP_BOOST_ENABLE);
    if (rc) {
        goto err;
    }

    //TODO lra specific?
    rc = drv2605_write8(itf, DRV2605_CONTROL3_ADDR, DRV2605_CONTROL3_LRA_DRIVE_MODE_ONCE | DRV2605_CONTROL3_LRA_OPEN_LOOP_CLOSED);
    if (rc) {
        goto err;
    }

    //previously computed
    rc = drv2605_write8(itf, DRV2605_AUTO_CALIBRATION_COMPENSATION_RESULT_ADDR, MYNEWT_VAL(DRV2605_CALIBRATED_COMP));
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_AUTO_CALIBRATION_BACK_EMF_RESULT_ADDR, MYNEWT_VAL(DRV2605_CALIBRATED_BEMF));
    if (rc) {
        goto err;
    }

    //TODO select for ERM too
    //Library 6 is a closed-loop library tuned for LRAs. The library selection occurs through register 0x03 (see the (Address: 0x03) section).
    rc = drv2605_write8(itf, DRV2605_WAVEFORM_CONTROL_ADDR, DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_LRA);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
drv2605_validate_cal(struct drv2605_cal *cal)
{
    int rc;

    if (cal->brake_factor > DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_MAX) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    if (cal->loop_gain > DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_MAX) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    if (cal->lra_sample_time > DRV2605_CONTROL2_SAMPLE_TIME_MAX) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    if (cal->lra_blanking_time > DRV2605_BLANKING_TIME_MAX) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    if (cal->lra_idiss_time > DRV2605_IDISS_TIME_MAX) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    if (cal->auto_cal_time > DRV2605_CONTROL4_AUTO_CAL_TIME_MAX) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    if (cal->lra_zc_det_time > DRV2605_CONTROL4_ZC_DET_TIME_MAX) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    return 0;
err:
    return rc;
}

// if succesful calibration overrites DRV2605_BEMF_GAIN, DRV2605_CALIBRATED_COMP and DRV2605_CALIBRATED_BEMF
// if successful restores mode bit
int
drv2605_auto_calibrate(struct sensor_itf *itf, struct drv2605_cal *cal)
{
    int rc;
    uint8_t temp;
    uint8_t interval = 255;

    uint8_t last_mode;
    uint8_t last_fb;

    rc = drv2605_validate_cal(cal);
    if (rc) {
        goto err;
    }

    rc = drv2605_read8(itf, DRV2605_MODE_ADDR, &last_mode);
    if (rc) {
        goto err;
    }

    rc = drv2605_read8(itf, DRV2605_FEEDBACK_CONTROL_ADDR, &last_fb);
    if (rc) {
        goto err;
    }

    // technically only need to protect the ERM_LRA bit as DRV2605_BEMF_GAIN will be altered anyway, but lets show em our fancy bit math
    uint8_t mask = (DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_MASK | DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_MASK);
    uint8_t altered = (cal->brake_factor << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS) | (cal->loop_gain << DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_POS);
    uint8_t new = (last_fb & (~mask)) | altered;
    rc = drv2605_write8(itf, DRV2605_FEEDBACK_CONTROL_ADDR, new);
    if (rc) {
        goto err;
    }

    int8_t idiss_lsb = cal->lra_idiss_time & 0x03;
    int8_t blanking_lsb = cal->lra_blanking_time & 0x03;
    int8_t ctrl2 = (cal->lra_sample_time << DRV2605_CONTROL2_SAMPLE_TIME_POS) | (blanking_lsb << DRV2605_CONTROL2_BLANKING_TIME_LSB_POS) | (idiss_lsb << DRV2605_CONTROL2_IDISS_TIME_LSB_POS);
    rc = drv2605_write8(itf, DRV2605_CONTROL2_ADDR, ctrl2);
    if (rc) {
        goto err;
    }

    int8_t blanking_msb = cal->lra_blanking_time & 0x0C;
    int8_t idiss_msb = cal->lra_idiss_time & 0x0C;
    rc = drv2605_write8(itf, DRV2605_CONTROL5_ADDR, blanking_msb  << DRV2605_CONTROL5_BLANKING_TIME_MSB_POS | idiss_msb  << DRV2605_CONTROL5_IDISS_TIME_MSB_POS );
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_CONTROL4_ADDR, (cal->lra_zc_det_time << DRV2605_CONTROL4_ZC_DET_TIME_POS) | (cal->auto_cal_time << DRV2605_CONTROL4_AUTO_CAL_TIME_POS));
    if (rc) {
        goto err;
    }

    //2. Write a value of 0x07 to register 0x01. This value moves the DRV2605L device out of STANDBY and places the MODE[2:0] bits in auto-calibration mode.
    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_AUTO_CALIBRATION);
    if (rc) {
        goto err;
    }

    //4. Set the GO bit (write 0x01 to register 0x0C) to start the auto-calibration process.
    rc = drv2605_write8(itf, DRV2605_GO_ADDR, DRV2605_GO_GO);
    if (rc) {
        goto err;
    }

    //When auto calibration is complete, the GO bit automatically clears. The auto-calibration results are written in the respective registers as shown in Figure 25.
    do{
        os_time_delay((OS_TICKS_PER_SEC * 5)/1000 + 1);
        rc = drv2605_read8(itf, DRV2605_GO_ADDR, &temp);
    } while (!rc && interval-- && (temp & DRV2605_GO_GO));

    //if we timed out
    if (!interval) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    //5. Check the status of the DIAG_RESULT bit (in register 0x00) to ensure that the auto-calibration routine is complete without faults
    rc = drv2605_read8(itf, DRV2605_STATUS_ADDR, &temp);
    if (rc || (temp & DRV2605_STATUS_DIAG_RESULT_FAIL)) {
        rc = 1; //TODO what code to return?
        goto err;
    }

    //return to mode at start
    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, last_mode | DRV2605_MODE_MODE_POS);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
drv2605_config(struct drv2605 *drv2605)
{
    int rc;
    uint8_t id;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    /* Check if we can read the chip address */
    rc = drv2605_get_chip_id(itf, &id);
    if (rc) {
        goto err;
    }

    if (id != DRV2605_STATUS_DEVICE_ID_2605 && id != DRV2605_STATUS_DEVICE_ID_2605L) {
        os_time_delay((OS_TICKS_PER_SEC * 100)/1000 + 1);

        rc = drv2605_get_chip_id(itf, &id);
        if (rc) {
            goto err;
        }

        if (id != DRV2605_STATUS_DEVICE_ID_2605 && id != DRV2605_STATUS_DEVICE_ID_2605L) {
            rc = SYS_EINVAL;
            goto err;
        }
    }

    rc = drv2605_send_defaults(itf);
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return (rc);
}

int
drv2605_load(struct sensor_itf *itf, uint8_t* wav_ids, size_t len)
{
    int rc;

    if (len > 8) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = drv2605_writelen(itf, DRV2605_WAVEFORM_SEQUENCER_ADDR, wav_ids, len);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

//note DOES NOT restore mode bit as is non blocking TODO, take async callback handler to inform of completion and allow mode change
int
drv2605_internal_trigger(struct sensor_itf *itf)
{
    int rc;

    //out of standby and Internal trigger
    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_INTERNAL_TRIGGER | DRV2605_MODE_ACTIVE);
    if (rc) {
        goto err;
    }

    // Trigger
    rc = drv2605_write8(itf, DRV2605_GO_ADDR, DRV2605_GO_GO);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}