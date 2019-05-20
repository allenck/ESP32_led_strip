// =========================================================================
// Released under the MIT License
// Copyright 2017-2018 Natanael Josue Rabello. All rights reserved.
// For the license information refer to LICENSE file in root directory.
// =========================================================================

/**
 * @file mpu_i2c.cpp
 * Example on how to setup MPU through I2C for basic usage.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "I2Cbus.hpp"
#include "MPU.hpp"
#include "mpu/math.hpp"
#include "mpu/types.hpp"

#include "strip.h"

static const char* TAG = "example";

static constexpr gpio_num_t SDA = GPIO_NUM_19;
static constexpr gpio_num_t SCL = GPIO_NUM_18;
static constexpr uint32_t CLOCK_SPEED = 400000;  // range from 100 KHz ~ 400Hz

extern "C" void app_main() {
    printf("$ MPU Driver Example: MPU-I2C\n");
    fflush(stdout);

    // Initialize I2C on port 0 using I2Cbus interface
    i2c0.begin(SDA, SCL, CLOCK_SPEED);

    // Or directly with esp-idf API
    /*
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = SDA;
    conf.scl_io_num = SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = CLOCK_SPEED;
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));
    */

    MPU_t MPU;  // create a default MPU object
    MPU.setBus(i2c0);  // set bus port, not really needed since default is i2c0
    MPU.setAddr(mpud::MPU_I2CADDRESS_AD0_LOW);  // set address, default is AD0_LOW

    // Great! Let's verify the communication
    // (this also check if the connected MPU supports the implementation of chip selected in the component menu)
    while (esp_err_t err = MPU.testConnection()) {
        ESP_LOGE(TAG, "Failed to connect to the MPU, error=%#X", err);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "MPU connection successful!");

    // Initialize
    ESP_ERROR_CHECK(MPU.initialize());  // initialize the chip and set initial configurations
    // Setup with your configurations
    // ESP_ERROR_CHECK(MPU.setSampleRate(50));  // set sample rate to 50 Hz
    // ESP_ERROR_CHECK(MPU.setGyroFullScale(mpud::GYRO_FS_500DPS));
    // ESP_ERROR_CHECK(MPU.setAccelFullScale(mpud::ACCEL_FS_4G));

    // Reading sensor data
    printf("Reading heading data:\n");
    //mpud::raw_axes_t accelRaw;   // x, y, z axes as int16
    //mpud::raw_axes_t gyroRaw;    // x, y, z axes as int16
    //mpud::float_axes_t accelG;   // accel axes in (g) gravity format
    //mpud::float_axes_t gyroDPS;  // gyro axes in (DPS) º/s format
    int16_t x, y, z;
    MPU.setAuxI2CEnabled(true); // needed for compass
    Strip strip = Strip(22, 22, NEO_RGB, 0);
    strip.begin();
    uint16_t min[3] = {0, 0,0};
    uint16_t max[3] = {0, 0,0};
    while (true) {
        // Read
#if 0
        MPU.acceleration(&accelRaw);  // fetch raw data from the registers
        MPU.rotation(&gyroRaw);       // fetch raw data from the registers
        // MPU.motion(&accelRaw, &gyroRaw);  // read both in one shot
        // Convert
        accelG = mpud::accelGravity(accelRaw, mpud::ACCEL_FS_4G);
        gyroDPS = mpud::gyroDegPerSec(gyroRaw, mpud::GYRO_FS_500DPS);
        // Debug
        printf("accel: [%+6.2f %+6.2f %+6.2f ] (G) \t", accelG.x, accelG.y, accelG.z);
        printf("gyro: [%+7.2f %+7.2f %+7.2f ] (º/s)\n", gyroDPS[0], gyroDPS[1], gyroDPS[2]);
#endif
        esp_err_t rtn = MPU.heading(&x, &y, &z);
        if(rtn != ESP_OK)
         ESP_LOGI(TAG, "heading failed: %s", esp_err_to_name(rtn));
        if(x < min[0]) min[0] = x;
				if(x > max[0]) max[0] = x;
				if(y < min[1]) min[1] = y;
				if(y > max[1]) max[1] = y;
				if(z < min[2]) min[2] = z;
				if(z > max[2]) max[2] = z;
        uint16_t range[3];
        range[0] = max[0]=min[0];
				range[1] = max[1]=min[1];
				range[2] = max[2]=min[2];
        if(range[0] == 0 || range[1] == 0 || range[2] == 0) continue;
        uint32_t c = ((((x-min[0])*256)/range[0])&0xff) << 16 | ((((y-min[1])*256)/range[1])&0xff) << 8 | ((((z-min[2])*256)/range[2] )&0xff);
        printf("heading: x:%d(%u), y:%d(%u), z:%d(%u)\n", x,(c&0xff0000)>>16,y,(c&0xff00)>>8,z, c&0xff);
				strip.fill(c, 0, 23);
        strip.show();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

