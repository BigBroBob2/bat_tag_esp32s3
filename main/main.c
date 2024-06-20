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
    // printf("ICM_count=%d,%d\n",ICM_count, imu_buf[0]);
    fwrite(&imu_buf,sizeof(short),ICM_count,f_imu);

    gpio_set_level(16,0);
  }
  }
}

void app_main(void)
{
    esp_err_t ret;

    gpio_set_direction(16,GPIO_MODE_OUTPUT);
    ////////////////////// Init config
    // init sdcard
    sdmmc_init();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // init IMU
    ICM_SPI_config();
    ICM_configSensor();
    ICM_enableINT();
    ICM_enableSensor();

    // init microphone
    i2s_init();

    /////////////////////// test sd card example
    // First create a file.
    const char *file_hello = MOUNT_POINT"/hello.txt";
    char example_data[EXAMPLE_MAX_CHAR_SIZE];
    snprintf(example_data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Hello", card->cid.name);
    ret = s_example_write_file(file_hello, example_data);
    if (ret != ESP_OK) {
        return;
    }

    const char *file_foo = MOUNT_POINT"/foo.txt";
    // Check if destination file exists before renaming
    struct stat st;
    if (stat(file_foo, &st) == 0) {
        // Delete it if it exists
        unlink(file_foo);
    }

    // Rename original file
    printf("Renaming file %s to %s\n", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0) {
        return;
    }

    ret = s_example_read_file(file_foo);

    ////////////////////// finish testing sdcard
    define_files();

    f_mic = fopen(mic_filename, "wb+");
    f_video = fopen(video_filename, "wb+");
    f_imu = fopen(imu_filename, "wb+");
    f_H_imu = fopen(H_imu_filename, "wb+");

    // test writing into fie speed
    // short data[1];
    // data[0] = 5;
    // FILE *f = fopen(imu_filename, "wb+");
    // while (1) {
    //   gpio_set_level(16,1);
    //   // fs_write(imu_filename, &data,sizeof(short),1);
    //   fwrite(&data,sizeof(short),1,f);

    //   gpio_set_level(16,0);
    // }
    // fclose(f);

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
    while (count < 10) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      count = count + 1;

      // xSemaphoreGive(sdcard_thread_semaphore);
      // portYIELD();
    }

    fclose(f_mic);
    fclose(f_video);
    fclose(f_imu);
    fclose(f_H_imu);
    
    printf("Recording finished!\n");

    // delete IMU thread
    gpio_uninstall_isr_service();
    
    vTaskDelete(mic_thread_handle);
    vTaskDelete(imu_thread_handle);
    vTaskDelete(sdcard_thread_handle);

    // i2s_delete();
    // ICM_SPI_delete();
    // sdmmc_delete();
}
