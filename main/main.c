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

#include <sys/stat.h>
#include <dirent.h>

#ifndef BLE_H
#define BLE_H
#include "bleconn.h"
#endif

#include "icm42688.h"

#include "microphone.h"

#ifndef SD_H
#define SD_H
#include "sdcard.h"
#endif



// bool if_first = true;


void sdcard_thread(void *p) {
  while (1) {
  if (xSemaphoreTake(sdcard_thread_semaphore,1)) {
    // if (!xSemaphoreTake(trial_finish_semaphore,1)) {
    gpio_set_level(16,1);

    // audio
    // fwrite(&mic_t,sizeof(uint32_t),1,f_mic);
    block_read_out_buf(&mic_circle_buf,mic_readout_buf,&mic_count);
    fwrite(&mic_readout_buf,sizeof(uint8_t),mic_count*mic_block_size,f_mic);
    
    // camera

    // IMU
    read_out_buf(&imu_circle_buf,imu_buf,&ICM_count);
    fwrite(&imu_buf,sizeof(short),ICM_count,f_imu);

    gpio_set_level(16,0);

    // xSemaphoreGive(sdcard_finish_semaphore);
    // portYIELD();

  //   }
  //   else {
  //       xSemaphoreGive(answer_trial_finish_semaphore);
  //       portYIELD();

  //       while (1) {
  //           vTaskDelay(pdMS_TO_TICKS(1000));
  //       }
  //   }
    
  }
}
}

///////////////////////////////// ble
static bool trial_start = false;

// In app_main() we call BLE_set_rcv_callback() to set up this function to be
// called whenever we receive BLE data.
void rcv_callback(char *str)
{
	printf("Received string: '%s'\n", str);
	BLE_printf("Got string '%s' from you!!", str);
}

void app_main(void)
{ 
    int count = 0;
    esp_err_t ret;

    /// trial finish semaphore
    trial_finish_semaphore = xSemaphoreCreateBinary();
    answer_trial_finish_semaphore = xSemaphoreCreateBinary();

    gpio_set_direction(16,GPIO_MODE_OUTPUT);
    gpio_set_direction(8,GPIO_MODE_OUTPUT);

    ////////////////////// init ble
    BLE_init("BatTag-BLE-spp");
    // BLE_set_rcv_callback(rcv_callback);

    // while (1) {vTaskDelay(pdMS_TO_TICKS(1000));}

    ////////////////////// Init config
    // init sdcard
    sdmmc_init();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // set trial_count as the initial file number
    trial_count = count_file_number();

    ///// wait for ble connection
    count = 0;
    while (!conn_handle_subs[1]) {
      printf("%03d, waiting for ble connection..\n", count);
      vTaskDelay(pdMS_TO_TICKS(1000));
      count = count + 1;
    }
    printf("Trial %02d ble connected!\n", trial_count);
    ///// wait for ble command
    // count = 0;
    // while (!trial_start & conn_handle_subs[1]) {
    //   printf("%03d, Trial %02d, waiting for command..\n",count,trial_count);
    //   vTaskDelay(pdMS_TO_TICKS(1000));
    //   count = count + 1;
    // }

    // init IMU
    ICM_SPI_config();
    ICM_configSensor();
    ICM_enableINT();
    ICM_enableSensor();

    // init microphone
    i2s_init();

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

    /// need to test mic data 
    // int i = 0;
    // for (i=0;i<100;i++) {
    // i2s_read(I2S_NUM_0, &PDMDataBuffer_1[0], AUDIO_BUF_BYTES, &bytes_read, portMAX_DELAY);
    
    // printf("%04d\n",PDMDataBuffer_1[0]);
    //     if(bytes_read != AUDIO_BUF_BYTES) {
    //         printf("bytes_read=%d\n",bytes_read);
    //     }
    // fwrite(&PDMDataBuffer_1[0],sizeof(uint16_t),AUDIO_BUF_BYTES/sizeof(uint16_t),f_mic);
    // }

    // fclose(f_mic);

    // while (1) {
    //   printf("stopped\n");
    //   vTaskDelay(pdMS_TO_TICKS(1000));
    // }

    ////////////////////// start thread
    // create sdcard thread
    sdcard_thread_semaphore = xSemaphoreCreateBinary();
    sdcard_finish_semaphore = xSemaphoreCreateBinary();
    xTaskCreate(sdcard_thread,"sdcard_thread",2048,NULL,(4|portPRIVILEGE_BIT),&sdcard_thread_handle);

    // create IMU thread
    imu_thread_semaphore = xSemaphoreCreateBinary();
    xTaskCreate(imu_thread,"imu_thread",2048,NULL,(4|portPRIVILEGE_BIT),&imu_thread_handle);

    // create IMU isr
    gpio_set_direction(IMU_PIN_INT,GPIO_MODE_INPUT);
    gpio_set_intr_type(IMU_PIN_INT,GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(IMU_PIN_INT,imu_isr_handler,NULL);

    // create mic switch buffer thread
    mic_read_semaphore = xSemaphoreCreateBinary();
    mic_read_finish_semaphore = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    // vTaskDelay(pdMS_TO_TICKS(1000));
    
    // xTaskCreate(mic_switchbuffer_thread,"mic_switchbuffer_thread",2048, NULL,(1|portPRIVILEGE_BIT),&mic_switchbuffer_thread_handle);

    // start mic read thread
    xTaskCreate(mic_read_thread,"mic_read_thread",2048, NULL,(4|portPRIVILEGE_BIT),&mic_read_thread_handle);

    // count = 0;
    while (conn_handle_subs[1]) {
      

      xSemaphoreGive(sdcard_thread_semaphore);
      portYIELD();

      vTaskDelay(pdMS_TO_TICKS(50));

      // count = count + 1;

      // xSemaphoreGive(sdcard_thread_semaphore);
      // portYIELD();
    }
    
    // printf("Recording finished!\n");

    // xSemaphoreGive(trial_finish_semaphore);
    // while(!xSemaphoreTake(answer_trial_finish_semaphore,1)){;}
    // stop imu and mic data reading
    i2s_delete();
    gpio_uninstall_isr_service();
    vTaskDelete(imu_thread_handle);
    vTaskDelete(mic_read_thread_handle);
    
    printf("Trial %03d finished!!\n",trial_count);

    vTaskDelay(pdMS_TO_TICKS(1000));

    
    // vTaskDelete(mic_switchbuffer_thread_handle);
    // vTaskDelete(sdcard_thread_handle);

    // vSemaphoreDelete(imu_thread_semaphore);
    // vSemaphoreDelete(mic_read_semaphore);
    // vSemaphoreDelete(sdcard_finish_semaphore);
    // vSemaphoreDelete(mic_read_finish_semaphore);
    // vSemaphoreDelete(sdcard_thread_semaphore);

    fclose(f_mic);
    fclose(f_video);
    fclose(f_imu);
    fclose(f_H_imu);

    while (1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      printf("finished!\n");
    }
}
