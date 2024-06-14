#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"

#ifndef CB_H
#define CB_H
#include "circular_buffer.h"
#endif

#ifndef SD_H
#define SD_H
#include "sdcard.h"
#endif

// register map
//  Bank 0
#define ICM_INT_CONFIG 20
#define ICM_INT_SOURCE0 101
#define ICM_INT_CONFIG1 100

#define ICM_GYRO_CONFIG0 79
#define ICM_ACCEL_CONFIG0 80

#define ICM_ACCEL_DATA_X1 31
#define ICM_ACCEL_DATA_Y1 33
#define ICM_ACCEL_DATA_Z1 35
#define ICM_GYRO_DATA_X1 37
#define ICM_GYRO_DATA_Y1 39
#define ICM_GYRO_DATA_Z1 41

#define ICM_PWR_MGMT0 78

#define ICM_SELF_TEST_CONFIG 112
#define ICM_WHO_AM_I 117
#define ICM_REG_BANK_SEL 118

// Bank 1
#define ICM_SENSOR_CONFIG 3

#define ICM_XG_ST_DATA 95
#define ICM_YG_ST_DATA 96
#define ICM_ZG_ST_DATA 97

// Bank 2
#define ICM_XA_ST_DATA 59
#define ICM_YA_ST_DATA 60
#define ICM_ZA_ST_DATA 61

// config
#define ICM_SPI_WRITE 0b00000000
#define ICM_SPI_READ 0b10000000

////////////////////////// config

// full-scale range of accel and gyro sensors
const int8_t ICM_AccelRange_idx = 0; // choose 0=+-16G, 1=+-8g, 2=+-4g, 3=+-2g
const int8_t ICM_GyroRange_idx = 0;  // choose 0=+-2000dps, 1=+-1000dps, 2=+-500dps, 3=+-250dps, 4=+-125dps

// sampling rate
const int8_t ICM_rate_idx = 6;

// Return the gyro full-scale range, in Degrees per Second,
// for each of the range selection settings.
double ICM_GyroRangeVal_dps(int8_t range_idx)
{
  switch (range_idx) {
    case 0: return 2000; break;
    case 1: return 1000; break;
    case 2: return  500; break;
    case 3: return  250; break;
    case 4: return  125; break;
    case 5: return   62.5; break;
    case 6: return  31.25; break;
    case 7: return  15.625; break;
    default: return 0; break;
  }
}

// Return the accelerometer full-scale range, in G,
// for each of the range selection settings.
// Note that the DATASHEET IS WRONG!!!
// The max range is 32G, NOT 16G!!!
//
double ICM_AccelRangeVal_G(int8_t range_idx)
{
  switch(range_idx) {
//    case 0: return 16;   // MANUAL IS WRONG!!! Off by factor of 2.
//    case 1: return 8;
//    case 2: return 4;
//    case 3: return 2;

    case 0: return 32;     // Correct mapping.
    case 1: return 16;
    case 2: return 8;
    case 3: return 4;
    case 4: return 2;      // Yes, it does go down to +/-2G, and even lower for higher index.
    default: return 0;
  }
}

// Return DataRate 
double ICM_SampRate(int8_t rate_idx)
{
  switch(rate_idx) {
    case 1: return 32000;
    case 2: return 16000;
    case 3: return  8000;
    case 4: return  4000;
    case 5: return  2000;
    case 6: return  1000;
    case 7: return   200;
    case 8: return   100;
    case 9: return    50;
    case 10: return   25;
    case 11: return   12.5;
    case 12: return    6.25;
    case 13: return    3.125;
    case 14: return    1.5625;
    case 15: return  500;
    default: return 0;
  }
}

// Convert integer ADC values to floating point scaled values.
double ICM_ADC2Float(double val) {
    return val / 32768.0;
}

//////////// connection define

spi_device_handle_t imu_device_handle = NULL;

#define IMU_PIN_NUM_SCLK 4
#define IMU_PIN_NUM_MOSI 5
#define IMU_PIN_NUM_MISO 6
#define IMU_PIN_NUM_CS 7
#define IMU_PIN_INT 15

short IMU_data[9] = {0};

//////////////////////// SPI operations

void ICM_SPI_config() {
  // Initialize SPI bus
  spi_bus_config_t buscfg = {
        .sclk_io_num = IMU_PIN_NUM_SCLK,
        .mosi_io_num = IMU_PIN_NUM_MOSI,
        .miso_io_num = IMU_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16 * sizeof(uint8_t)
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // add ICM42688P to spi bus
    // esp32s3 can do at most 80MHz, ICM-42688-P can do at most 24MHz
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 24000000,
        .mode = 0,
        .spics_io_num = IMU_PIN_NUM_CS,
        .queue_size = 2
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &imu_device_handle));
}

void ICM_SPI_delete() {
  spi_bus_remove_device(&imu_device_handle);
  vTaskDelay(pdMS_TO_TICKS(1000));
  spi_bus_free(SPI2_HOST);
}

void ICM_configSensor() {
  //config gyro
  uint8_t txrx_buf[2];
  txrx_buf[0] = ICM_SPI_WRITE | ICM_GYRO_CONFIG0;
  txrx_buf[1] = (ICM_GyroRange_idx << 5) | ICM_rate_idx;

  spi_transaction_t spi_trans = {
      .tx_buffer = &txrx_buf,
      .length = 2*8
  };
  spi_device_transmit(imu_device_handle,&spi_trans);

  // config accel
  txrx_buf[0] = ICM_SPI_WRITE | ICM_ACCEL_CONFIG0;
  txrx_buf[1] = (ICM_AccelRange_idx << 5) | ICM_rate_idx;

  spi_device_transmit(imu_device_handle,&spi_trans);
}

void ICM_enableSensor() {
  // write to enable accel and gyro
    uint8_t txrx_buf[2];
    txrx_buf[0] = ICM_SPI_WRITE | ICM_PWR_MGMT0;
    txrx_buf[1] = 0b00001111;

    spi_transaction_t spi_trans = {
        .tx_buffer = &txrx_buf,
        .length = 2*8
    };
    spi_device_transmit(imu_device_handle,&spi_trans);
}

void ICM_enableINT() {
    uint8_t txrx_buf[2];

    //ICM_INT_CONFIG
    // Bit 0 set 1: INT1-active high
    // Bit 1 set 1: INT1-push-pull
    // Bit 2 set 0: INT1 pulse
    txrx_buf[0] = ICM_SPI_WRITE | ICM_INT_CONFIG;
    txrx_buf[1] = 0b00000011;

    spi_transaction_t spi_trans = {
        .tx_buffer = &txrx_buf,
        .length = 2*8
    };
    spi_device_transmit(imu_device_handle,&spi_trans);

    //ICM_INT_CONFIG1
    // Bit 5 set 1: Disables de-assert duration. Required if ODR ≥ 4kHz
    // Bit 6 set 1:  Interrupt pulse duration is 8 μs. Required if ODR ≥ 4kHz,
    txrx_buf[0] = ICM_SPI_WRITE | ICM_INT_CONFIG1;
    txrx_buf[1] = 0b01100000;
    spi_device_transmit(imu_device_handle,&spi_trans);

    //ICM_INT_SOURCE0
    // Bit 3 set 1:  UI data ready interrupt routed to INT1
    txrx_buf[0] = ICM_SPI_WRITE | ICM_INT_SOURCE0;
    txrx_buf[1] = 0b00001000;
    spi_device_transmit(imu_device_handle,&spi_trans);
}

void ICM_readSensor() {
  // read one sample from ICM42688P
    uint8_t txrx_buf[13];
    txrx_buf[0] = ICM_SPI_READ | ICM_ACCEL_DATA_X1;

    spi_transaction_t spi_trans = {
        .tx_buffer = &txrx_buf,
        .length = 13*8,
        .rx_buffer = &txrx_buf,
        .rxlength = 13*8
    };
    spi_device_transmit(imu_device_handle,&spi_trans);

    // spi_transaction_t spi_trans = {
    //     .tx_buffer = &txrx_buf,
    //     .length = 8
    // };
    // spi_device_queue_trans(imu_device_handle,&spi_trans,portMAX_DELAY);
    // spi_trans.rx_buffer = &txrx_buf;
    // spi_trans.length = 13*8;
    // spi_trans.rxlength = 13*8;
    // spi_device_get_trans_result(imu_device_handle,&spi_trans,portMAX_DELAY);

    IMU_data[0] = (short)((txrx_buf[1] << 8)  | txrx_buf[2]);
    IMU_data[1] = (short)((txrx_buf[3] << 8)  | txrx_buf[4]);
    IMU_data[2] = (short)((txrx_buf[5] << 8)  | txrx_buf[6]);
    IMU_data[3] = (short)((txrx_buf[7] << 8)  | txrx_buf[8]);
    IMU_data[4] = (short)((txrx_buf[9] << 8)  | txrx_buf[10]);
    IMU_data[5] = (short)((txrx_buf[11] << 8) | txrx_buf[12]);
}

//////////////////////// circular buffer
#define N_imu_circular_buf 2048
// IMU circular buffer
static short imu_cbuf[N_imu_circular_buf]; 
// IMU sample data circular buffer
static circular_buf imu_circle_buf = {
    .N = N_imu_circular_buf,
    .buf = imu_cbuf,
    .write_idx = 0,
    .read_idx = 0
};

//////////////////////// IMU task, thread, semaphore
TaskHandle_t imu_thread_handle = NULL;
SemaphoreHandle_t imu_thread_semaphore = NULL;

// timestamp for each IMU sample
static int32_t imu_t;
// IMU buffer to transfer IMU sample from circular buffer to sd card
static short imu_buf[N_imu_circular_buf];
// sample count within the buffer, be really careful with uint8_t and int8_t
static uint16_t ICM_count = 0;

void imu_thread(void *p) {
  while (1) {
  if (xSemaphoreTake(imu_thread_semaphore,1)) {
    imu_t = (uint32_t)esp_timer_get_time();
    ICM_readSensor();

    IMU_data[6] = imu_t >> 16;
    IMU_data[7] = imu_t & 0x0000FFFF;
    IMU_data[8]++; 

    write_in_buf(&imu_circle_buf,IMU_data,9);
  }
  }
}

/////////////////////// ISR

void imu_isr_handler() {
  xSemaphoreGiveFromISR(imu_thread_semaphore,NULL);
  portYIELD_FROM_ISR(1);
}

///////////////////////////////////////////////////////




