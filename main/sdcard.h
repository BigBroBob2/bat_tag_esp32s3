#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#define EXAMPLE_MAX_CHAR_SIZE    64

#define MOUNT_POINT "/sd"

sdmmc_card_t *card;
const char mount_point[] = MOUNT_POINT;

void sdmmc_init() {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = 20000;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;

    // connection pin
    slot_config.clk = 11;
    slot_config.cmd = 12;
    slot_config.d0 = 38;
    slot_config.d1 = 39;
    slot_config.d2 = 40;
    slot_config.d3 = 41;

    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_ERROR_CHECK(esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card));

    sdmmc_card_print_info(stdout, card);

    ESP_ERROR_CHECK(esp_vfs_fat_sdcard_format(mount_point, card));
}

void sdmmc_delete() {
    esp_vfs_fat_sdcard_unmount(mount_point, card);
}

///////////////////// files

static int trial_count = 0;

//// file names
char mic_filename[32];
char video_filename[32];
char imu_filename[32];
char H_imu_filename[32];

void define_files() {
    // create files

    memset(mic_filename, 0, 32);
    snprintf(mic_filename, 32, MOUNT_POINT"/audio_%02d.dat", trial_count);
    FILE *f = fopen(mic_filename, "wb+");
    fclose(f);

    memset(video_filename, 0, 32);
    snprintf(video_filename, 32, MOUNT_POINT"/video_%02d.dat", trial_count);
    f = fopen(video_filename, "wb+");
    fclose(f);

    memset(imu_filename, 0, 32);
    snprintf(imu_filename, 32, MOUNT_POINT"/imu_%02d.dat", trial_count);
    f = fopen(imu_filename, "wb+");
    fclose(f);

    memset(H_imu_filename, 0, 32);
    snprintf(H_imu_filename, 32, MOUNT_POINT"/h_imu_%02d.dat", trial_count);
    f = fopen(H_imu_filename, "wb+");
    fclose(f);
}

void fs_write(char *path, const void *ptr, size_t size, size_t nmemb) {
    FILE *f = fopen(path, "wb+");
    fwrite(ptr,size,nmemb,f);
    fclose(f);
}

////////////////////// sdcard thread

TaskHandle_t sdcard_thread_handle = NULL;
SemaphoreHandle_t sdcard_thread_semaphore = NULL;

void sdcard_thread(void *p);