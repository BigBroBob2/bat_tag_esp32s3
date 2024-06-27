#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "esp_log.h"

#ifndef SD_H
#define SD_H
#include "sdcard.h"
#endif

#define PDM_RX_PIN_CLK 17
#define PDM_RX_PIN_DIN 18

// real CLK freq = PDM_RX_FREQ_HZ*64
#define PDM_RX_FREQ_HZ 16000

#define SAMPLE_BIT_SIZE  I2S_BITS_PER_CHAN_32BIT
#define BYTES_PER_SAMPLE 4

#define AUDIO_BUF_SAMPLES 4096
#define AUDIO_BUF_NUM 2 // double buffering

#define DMA_BUF_BYTES 1024
#define DMA_BUF_NUM (AUDIO_BUF_SAMPLES*AUDIO_BUF_NUM/DMA_BUF_BYTES)

#define AUDIO_BUF_BYTES (AUDIO_BUF_SAMPLES*BYTES_PER_SAMPLE)

// https://github.com/oclyke/esp32-i2s-pdm-fft/blob/main/main/i2s_example_main.c
void i2s_init() {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX |I2S_MODE_PDM,
        .sample_rate =  PDM_RX_FREQ_HZ,
        .bits_per_sample = SAMPLE_BIT_SIZE,
        .communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .intr_alloc_flags = 0,
        .dma_buf_count = DMA_BUF_NUM, // 2-8
        .dma_buf_len = DMA_BUF_BYTES, // 128-1024
    };

    i2s_pin_config_t pin_config = {
        .ws_io_num   = PDM_RX_PIN_CLK,
        .data_in_num = PDM_RX_PIN_DIN,
    };

    //install and start i2s driver
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void i2s_delete() {
    i2s_driver_uninstall(I2S_NUM_0);
}

// timestamp for audio double buffer switching
static uint32_t mic_t;

static uint8_t PDMDataBuffer_1[AUDIO_BUF_BYTES];
static uint8_t PDMDataBuffer_2[AUDIO_BUF_BYTES];
int processing_Buffer_1 = 0;

TaskHandle_t mic_switchbuffer_thread_handle = NULL;
// SemaphoreHandle_t mic_switchbuffer_thread_semaphore = NULL;

TaskHandle_t mic_read_thread_handle = NULL;
SemaphoreHandle_t mic_read_semaphore = NULL;
SemaphoreHandle_t mic_read_finish_semaphore = NULL;

static size_t buf_idx = 0;
static size_t bytes_read = 0;
static uint8_t* buf = NULL;
static uint8_t* Buffer_to_process = &PDMDataBuffer_1[0];
static uint8_t* Buffer_is_processing = NULL;

void mic_read_thread(void *p) {
    while (1) {
        if (xSemaphoreTake(mic_read_semaphore,1)) {

        // gpio_set_level(8,1);    

        bytes_read = 0;

        // this should take enough long time to switch and write buffer
        // i2s_zero_dma_buffer(I2S_NUM_0);
        ESP_ERROR_CHECK(i2s_read(I2S_NUM_0, Buffer_is_processing, AUDIO_BUF_BYTES, &bytes_read, portMAX_DELAY));
        if(bytes_read != AUDIO_BUF_BYTES) {
            printf("bytes_read=%d\n",bytes_read);
        }

        // gpio_set_level(8,0);

        xSemaphoreGive(mic_read_finish_semaphore);
        }
    }
}

void mic_switchbuffer_thread(void *p) {
    
    while (1) {
        mic_t = (uint32_t)esp_timer_get_time();

        Buffer_is_processing = Buffer_to_process;
        xSemaphoreGive(mic_read_semaphore);
        portYIELD();
        // bytes_read = 0;
        // Buffer_is_processing = Buffer_to_process;

        // // this should take enough long time to switch and write buffer
        // ESP_ERROR_CHECK(i2s_read(I2S_NUM_0, Buffer_is_processing, AUDIO_BUF_BYTES, &bytes_read, portMAX_DELAY));
        // assert(bytes_read == AUDIO_BUF_BYTES);

        Buffer_to_process = processing_Buffer_1 ? &PDMDataBuffer_1[0] : &PDMDataBuffer_2[0];
        // switch double buffering
        processing_Buffer_1 = !processing_Buffer_1;

        xSemaphoreGive(sdcard_thread_semaphore);
        portYIELD();

        // mic_read should finish
        while (!xSemaphoreTake(mic_read_finish_semaphore,portMAX_DELAY)) {;}
        // // sdcard write should finish
        while (!xSemaphoreTake(sdcard_finish_semaphore,portMAX_DELAY)) {;}
    }
}
