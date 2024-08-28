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
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_52M;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;

    // connection pin
    slot_config.clk = 11;
    slot_config.cmd = 12;
    slot_config.d0 = 13;
    slot_config.d1 = 14;
    slot_config.d2 = 15;
    slot_config.d3 = 16;

    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_ERROR_CHECK(esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card));

    sdmmc_card_print_info(stdout, card);

    // ESP_ERROR_CHECK(esp_vfs_fat_sdcard_format(mount_point, card));
}

void sdmmc_delete() {
    esp_vfs_fat_sdcard_unmount(mount_point, card);
}

///////////////////// files

void fs_write(char *path, const void *ptr, size_t size, size_t nmemb) {
    FILE *f = fopen(path, "wb+");
    fwrite(ptr,size,nmemb,f);
    fclose(f);
}

static int trial_count = 0;

//// file names
static char mic_filename[32];
static char video_filename[32];
static char imu_filename[32];
static char H_imu_filename[32];

//// FILE
static FILE *f_mic;
static FILE *f_video;
static FILE *f_imu;
static FILE *f_H_imu;

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



////////////////////// sdcard thread

TaskHandle_t sdcard_thread_handle = NULL;
SemaphoreHandle_t sdcard_thread_semaphore = NULL;
SemaphoreHandle_t sdcard_finish_semaphore = NULL;

void sdcard_thread(void *p);


static esp_err_t s_example_write_file(const char *path, char *data)
{   
    char *TAG = "example";
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

static esp_err_t s_example_read_file(const char *path)
{   
    char *TAG = "example";
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

int count_file_number() {
    int file_number = 0;

    DIR *mydir;
    struct dirent *en;
    
    mydir = opendir(mount_point);

    while((en = readdir(mydir)) != NULL) {
        file_number = file_number + 1;
    }
    closedir(mydir);

    printf("Found %d files in SD card!\n",file_number);
    
    return file_number;
}