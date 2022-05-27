#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "config.h"
#include "configCmd.h"
#include "stdio.h"
#include "nvs_flash.h"

#define STORAGE_NAMESPACE    "ESP32_CONFIG"

#define START_CONFIG         0xDEADBEAF
#define END_CONFIG           0xBEAFDEAD

static const char * error_lvl_str[] = 
{
    [PRINT_DEBUG] = "DEBUG: ",
    [PRINT_INFO] = "INFO: ",
    [PRINT_WARNING] = "WARNING: ",
    [PRINT_ERROR] = "ERROR: "
};

config_t config =
{
    .start_config = START_CONFIG,
    .hw_ver       = BOARD_VERSION,
    .sw_ver       = SOFTWARE_VERSION,
    .dev_type     = DEFAULT_DEV_TYPE,
    .end_config   = END_CONFIG
};

static xSemaphoreHandle mutexSemaphore;

uint8_t test_config_save(void);

void config_printf(enum config_print_lvl module_lvl, enum config_print_lvl msg_lvl, const char *format, ...)
{
	if (module_lvl <= msg_lvl)
	{
        xSemaphoreTake(mutexSemaphore, 250);
        printf(error_lvl_str[msg_lvl]);
		va_list args;
		va_start(args, format);
		vprintf (format, args);
		va_end(args);
        printf("\n\r");
        xSemaphoreGive(mutexSemaphore);
	}
}

int verify_config(config_t * conf)
{
    if ((conf->start_config == START_CONFIG) && (conf->end_config == END_CONFIG))
    {
        if ((memcmp(&config.hw_ver, &conf->hw_ver, sizeof(config.hw_ver)) != 0) ||
            (memcmp(&config.sw_ver, &conf->sw_ver,
            sizeof(config.sw_ver)) != 0) || (memcmp(&config.name, &conf->name, sizeof(config.name)) != 0))
        {
            config.can_id = conf->can_id;
            config.dev_type = conf->dev_type;
            return 0;
        }

        return 1;
    }

    return 0;
}

static void configInitStruct(config_t *con)
{
    if (con->can_id == 0)
    {
        con->can_id = 0;
    }

    memcpy(con->name, BOARD_NAME, sizeof(BOARD_NAME));
    configSave(con);
}

void configInit(void)
{
    mutexSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(mutexSemaphore);
    // Read and verify config
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    config_t read_config;

#if CONFIG_USE_CONSOLE
    cosole_echo_flag = CONFIG_CONSOLE_ECHO;
#endif
    if (configRead(&read_config) == ESP_OK)
    {
        if (verify_config(&read_config) == TRUE)
        {
            memcpy(&config, &read_config, sizeof(config_t));
        }
        else
        {
            configInitStruct(&config);
        }

        if ((config.can_id <= 0) || (config.can_id >= 0x40))
        {
            config.can_id = 0;
        }
    }
    else
    {
        configInitStruct(&config);
    }
}

int configSave(config_t *config)
{
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        nvs_close(my_handle);
        return err;
    }

    err = nvs_set_blob(my_handle, "config", config, sizeof(config_t));

    if (err != ESP_OK)
    {
        nvs_close(my_handle);
        return err;
    }

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        nvs_close(my_handle);
        return err;
    }

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t configRead(config_t *config)
{
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS

    err = nvs_get_blob(my_handle, "config", NULL, &required_size);
    if ((err != ESP_OK) && (err != ESP_ERR_NVS_NOT_FOUND))
    {
        nvs_close(my_handle);
        return err;
    }

    // Read previously saved blob if available
    if (required_size == sizeof(config_t))
    {
        err = nvs_get_blob(my_handle, "config", config, &required_size);
        nvs_close(my_handle);
        return err;
    }

    nvs_close(my_handle);
    return ESP_ERR_NVS_NOT_FOUND;
}

void configRebootToBlt(void)
{
    esp_restart();
}
