/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
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

    // audio
    // fwrite(&mic_t,sizeof(uint32_t),1,f_mic);
    block_read_out_buf(&mic_circle_buf,mic_readout_buf,&mic_count);
    fwrite(&mic_readout_buf,sizeof(uint8_t),mic_count*mic_block_size,f_mic);
    
    // camera

    // IMU
    read_out_buf(&imu_circle_buf,imu_buf,&ICM_count);
    fwrite(&imu_buf,sizeof(short),ICM_count,f_imu);
    // printf("ICM_count=%d\n",ICM_count);
    // H_IMU
    read_out_buf(&H_imu_circle_buf,H_imu_buf,&H_ICM_count);
    fwrite(&H_imu_buf,sizeof(short),H_ICM_count,f_H_imu);

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
char *my_strtok(char *str, char sep)
{
	char *inc;

	// Search for sep character in string.
	for(inc=str; *inc && *inc!=sep; ++inc)
	   ;
	
	*inc = 0;    // Terminate string at sep character.
	return str;  // Return beginning of string.
}

static bool trial_start = false;

static bool enable_imu = true;
static bool enable_H_imu = true;
static bool enable_mic = true;
static bool enable_cam = true;

static int cam_interval = 50;

// In app_main() we call BLE_set_rcv_callback() to set up this function to be
// called whenever we receive BLE data.
void rcv_callback(char *cmdstr)
{
	printf("Received string: '%s'\n", cmdstr);
	BLE_printf("Got string '%s' from you!!", cmdstr);

  // Can come up with any command scheme you want. Here, we take an initial
	// '*' to mean that we should look for a command, then we search for a
	// matching command name.
	if(cmdstr[0] == '*') {
		char *cmd = my_strtok(cmdstr+1, ' '); // Get the first word after *
		int cmdlen = strlen(cmd);
		char *args = cmdstr+cmdlen+2; // remainder of string, after command token and space.

		printf("Found command: '%s' (%d chars), and args '%s'\n", cmd, cmdlen, args);

		// If we recognize the command, do something with it.
		if (strcmp(cmd, "stop") == 0) {
			// stop recording
			trial_start = false;
			// BLE_printf("Trial stop");
      return;
		}
    else if (strcmp(cmd, "start") == 0) {
			// start recording
			trial_start = true;
			// BLE_printf("Trial start");
      return;
		}
		else if(strcmp(cmd, "setp") == 0) {
			// get var name and value to set
			char *var = my_strtok(args, ' ');
			args = args+strlen(var)+1;
			char *value_str = my_strtok(args, ' ');

			// check legal 
			if (strcmp(var,"")==0 || strcmp(value_str,"")==0){
				BLE_printf("Invalid command");
				return;
			}

			int value = atoi(value_str);

			if (strcmp(var, "imu_fs") == 0) {
				if (value==1000) {ICM_rate_idx=6;}
				else if (value==200) {ICM_rate_idx=7;}
				else if (value==100) {ICM_rate_idx=8;}
				else if (value==500) {ICM_rate_idx=15;}
				else {
					BLE_printf("Invalid imu_fs value, set default value 100 Hz");
					ICM_rate_idx=8;
				}
			}
			else if (strcmp(var, "mic_fs") == 0) {
				if (value==4000000) {PDM_RX_FREQ_HZ=128000;}
				else if (value==2000000) {PDM_RX_FREQ_HZ=64000;}
				else if (value==1000000) {PDM_RX_FREQ_HZ=32000;}
				else if (value== 500000) {PDM_RX_FREQ_HZ=16000;}
				else {
					BLE_printf("Invalid mic_fs value, set default value 500000 Hz");
					PDM_RX_FREQ_HZ=16000;
				}
			}
			else if (strcmp(var, "cam_fs") == 0) {
				if (value==20) {cam_interval=50;}
				else if (value==10) {cam_interval=100;}
				else if (value==5) {cam_interval=200;}
				else {
					BLE_printf("Invalid cam_fs value, set default value 20 Hz");
					cam_interval=50;
				}
			}

			else if (strcmp(var, "enable_imu") == 0) {
				if (value==0) {enable_imu=0;}
				else if (value==1) {enable_imu=1;}
				else {
					BLE_printf("Invalid enable_imu value, set default value 1");
					enable_imu=1;
				}
			}
			else if (strcmp(var, "enable_H_imu") == 0) {
				if (value==0) {enable_H_imu=0;}
				else if (value==1) {enable_H_imu=1;}
				else {
					BLE_printf("Invalid enable_H_imu value, set default value 1");
					enable_H_imu=1;
				}
			}
			else if (strcmp(var, "enable_mic") == 0) {
				if (value==0) {enable_mic=0;}
				else if (value==1) {enable_mic=1;}
				else {
					BLE_printf("Invalid enable_mic value, set default value 1");
					enable_mic=1;
				}
			}
      else if (strcmp(var, "enable_cam") == 0) {
				if (value==0) {enable_cam=0;}
				else if (value==1) {enable_cam=1;}
				else {
					BLE_printf("Invalid enable_cam value, set default value 1");
					enable_cam=1;
				}
			}
			else {
				BLE_printf("Invalid command");
				return;
			}

			BLE_printf("Successfully set %s",var);
		}
		else {
			BLE_printf("Invalid command");
			return;
		}
	}
  else {
    BLE_printf("Invalid command");
    return;
  }
}

void app_main(void)
{ 
    int count = 0;
    esp_err_t ret;

    /// trial finish semaphore
    trial_finish_semaphore = xSemaphoreCreateBinary();
    answer_trial_finish_semaphore = xSemaphoreCreateBinary();

    // debug pin
    // gpio_set_direction(16,GPIO_MODE_OUTPUT);

    ////////////////////// init ble
    BLE_init("BatTag-BLE-spp");
    BLE_set_rcv_callback(rcv_callback);

    // while (1) {vTaskDelay(pdMS_TO_TICKS(1000));}

    ////////////////////// Init config
    // init sdcard
    sdmmc_init();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // set trial_count as the initial file number
    trial_count = count_file_number();

    // init imu spi only once
    ICM_SPI_config();
    // init mic i2s only once
    i2s_init();

    // while loop for disconnect and reconnect
    while (1) {
    ///// wait for ble connection
    count = 0;
    while (!conn_handle_subs[1]) {
      printf("%03d, waiting for ble connection..\n", count);
      vTaskDelay(pdMS_TO_TICKS(1000));
      count = count + 1;
    }
    printf("Trial %02d ble connected!\n", trial_count);

    // while loop for multiple trials within one connection
    while (conn_handle_subs[1]) {
      
    ///// wait for ble command to config and start
    count = 0;
    while (!trial_start && conn_handle_subs[1]) {
      printf("%03d, ble connected, Trial %02d waiting for command..\n",count,trial_count);
      vTaskDelay(pdMS_TO_TICKS(1000));
      count = count + 1;
    }

    // if disconnect, jump out of this loop
    if (!conn_handle_subs[1]) {
      continue;
    }

    printf("Trial %02d started recording!\n", trial_count);

    //// always reset circular buffer before next trial and reset sample idx count
    {
      imu_circle_buf.read_idx = 0;
      imu_circle_buf.write_idx = 0;
      IMU_data[8] = 0;

      H_imu_circle_buf.read_idx = 0;
      H_imu_circle_buf.write_idx = 0;
      H_IMU_data[8] = 0;

      mic_circle_buf.read_idx = 0;
      mic_circle_buf.write_idx = 0;
    }

    // init IMU
    if (enable_imu) {
      ICM_configSensor();
      ICM_enableINT();
      ICM_enableSensor();
    }

    // init H_IMU
    if (enable_H_imu) {
      H_ICM_configSensor();
      H_ICM_enableINT();
      H_ICM_enableSensor();
    }

    // init microphone
    // if (enable_mic) {
    //   i2s_init();
    // }

    ////////////////////// define trial files
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
    //   // fs_write(imu_filename, &data,sizeof(short),1);
    //   fwrite(&data,sizeof(short),1,f);

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
    xTaskCreate(sdcard_thread,"sdcard_thread",4096,NULL,((configMAX_PRIORITIES-1)|portPRIVILEGE_BIT),&sdcard_thread_handle);

    gpio_install_isr_service(0);
    // create IMU thread
    imu_thread_semaphore = xSemaphoreCreateBinary();
    if (enable_imu) {
      xTaskCreate(imu_thread,"imu_thread",2048,NULL,((configMAX_PRIORITIES-1)|portPRIVILEGE_BIT),&imu_thread_handle);

      // create IMU isr
      gpio_reset_pin(IMU_PIN_INT);
      gpio_set_direction(IMU_PIN_INT,GPIO_MODE_INPUT);
      gpio_set_intr_type(IMU_PIN_INT,GPIO_INTR_POSEDGE);
      gpio_isr_handler_add(IMU_PIN_INT,imu_isr_handler,NULL);
    }

    // create H_IMU thread
    H_imu_thread_semaphore = xSemaphoreCreateBinary();
    if (enable_H_imu) {
      xTaskCreate(H_imu_thread,"H_imu_thread",2048,NULL,((configMAX_PRIORITIES-1)|portPRIVILEGE_BIT),&H_imu_thread_handle);

      // create H_IMU isr
      gpio_reset_pin(H_IMU_PIN_INT);
      gpio_set_direction(H_IMU_PIN_INT,GPIO_MODE_INPUT);
      gpio_set_intr_type(H_IMU_PIN_INT,GPIO_INTR_POSEDGE);
      gpio_isr_handler_add(H_IMU_PIN_INT,H_imu_isr_handler,NULL);
    }

    // create mic switch buffer thread
    if (enable_mic) {
      mic_read_semaphore = xSemaphoreCreateBinary();
      mic_read_finish_semaphore = xSemaphoreCreateBinary();
      ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

      // vTaskDelay(pdMS_TO_TICKS(1000));
      
      // xTaskCreate(mic_switchbuffer_thread,"mic_switchbuffer_thread",2048, NULL,(1|portPRIVILEGE_BIT),&mic_switchbuffer_thread_handle);

      // start mic read thread
      xTaskCreate(mic_read_thread,"mic_read_thread",2048, NULL,((configMAX_PRIORITIES-1)|portPRIVILEGE_BIT),&mic_read_thread_handle);
    }

    // count = 0;
    while (trial_start && conn_handle_subs[1]) {
      

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
    if (enable_mic) {
      i2s_channel_disable(rx_chan);
    }

    gpio_uninstall_isr_service();
    if (enable_imu) {
      vTaskDelete(imu_thread_handle);
    }
    if (enable_H_imu) {
      vTaskDelete(H_imu_thread_handle);
    }

    if (enable_mic) {
      vTaskDelete(mic_read_thread_handle);
    }
    
    printf("Trial %03d finished recording!!\n",trial_count);

    vTaskDelay(pdMS_TO_TICKS(1000));

    
    // vTaskDelete(mic_switchbuffer_thread_handle);
    vTaskDelete(sdcard_thread_handle);

    // vSemaphoreDelete(imu_thread_semaphore);
    // vSemaphoreDelete(mic_read_semaphore);
    // vSemaphoreDelete(sdcard_finish_semaphore);
    // vSemaphoreDelete(mic_read_finish_semaphore);
    // vSemaphoreDelete(sdcard_thread_semaphore);

    fclose(f_mic);
    fclose(f_video);
    fclose(f_imu);
    fclose(f_H_imu);

    if (enable_imu) {
      ICM_disableSensor();
    }
    if (enable_H_imu) {
      H_ICM_disableSensor();
    }

    printf("Trial %02d All finished!\n",trial_count);
    trial_count = trial_count + 1;
    }
    }
}
