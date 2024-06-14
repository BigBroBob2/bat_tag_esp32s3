/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"


#include "icm42688.h"

#include "microphone.h"

#ifndef SD_H
#define SD_H
#include "sdcard.h"
#endif

void sdcard_thread(void *p) {
  while (1) {
  if (xSemaphoreTake(sdcard_thread_semaphore,1)) {
    // audio
    // fs_write(mic_filename, &mic_t,sizeof(uint32_t),1);
    // fs_write(mic_filename, &Buffer_to_process,sizeof(uint8_t),AUDIO_BUF_BYTES);
    

    // camera

    // IMU
    
    gpio_set_level(16,1);
    read_out_buf(&imu_circle_buf,imu_buf,&ICM_count);
    // fs_write(imu_filename, &imu_buf,sizeof(short),ICM_count);
    gpio_set_level(16,0);
  }
  }
}

void app_main(void)
{ 
    gpio_set_direction(16,GPIO_MODE_OUTPUT);
    ////////////////////// Init config
    // init sdcard
    // sdmmc_init();
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // init IMU
    ICM_SPI_config();
    ICM_configSensor();
    ICM_enableINT();
    ICM_enableSensor();

    // init microphone
    i2s_init();

    ////////////////////// start thread
    // create sdcard thread
    sdcard_thread_semaphore = xSemaphoreCreateBinary();
    xTaskCreate(sdcard_thread,"sdcard_thread",2048,NULL,(4|portPRIVILEGE_BIT),&sdcard_thread_handle);

    // create IMU thread
    imu_thread_semaphore = xSemaphoreCreateBinary();
    xTaskCreate(imu_thread,"imu_thread",2048,NULL,(4|portPRIVILEGE_BIT),&imu_thread_handle);

    // create IMU isr
    gpio_set_direction(IMU_PIN_INT,GPIO_MODE_INPUT);
    gpio_set_intr_type(IMU_PIN_INT,GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(IMU_PIN_INT,imu_isr_handler,NULL);

    // create mic thread
    xTaskCreate(mic_thread,"mic_thread",2048, NULL,(4|portPRIVILEGE_BIT),&mic_thread_handle);

    int count = 0;
    while (count < 100) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      count = count + 1;

      // xSemaphoreGive(sdcard_thread_semaphore);
      // portYIELD();
    }
    
    // delete IMU thread
    gpio_uninstall_isr_service();
    
    vTaskDelete(mic_thread_handle);
    vTaskDelete(imu_thread_handle);
    vTaskDelete(sdcard_thread_handle);

    i2s_delete();
    ICM_SPI_delete();
    // sdmmc_delete();
}
