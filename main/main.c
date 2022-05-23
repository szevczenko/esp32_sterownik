#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "config.h"

#include "wifidrv.h"
#include "cmd_server.h"
#include "but.h"
#include "fast_add.h"
#include "ssd1306.h"
#include "ssd1306_tests.h"
#include "menu.h"
#include "keepalive.h"
#include "menu_param.h"
#include "cmd_client.h"
#include "error_siewnik.h"
#include "measure.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "pcf8574.h"
#include "battery.h"
#include "motor.h"
#include "vibro.h"
#include "esp_attr.h"
#include "buzzer.h"
#include "esp_sleep.h"
#include "sleep_e.h"
#include "server_controller.h"
#include "power_on.h"

uint16_t test_value;
static gpio_config_t io_conf;
static uint32_t blink_pin = GPIO_NUM_25;

portMUX_TYPE portMux = portMUX_INITIALIZER_UNLOCKED;
extern void ultrasonar_start(void);

static int i2cInit(void)
{
    int i2c_master_port = SSD1306_I2C_PORT;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;
    conf.sda_pullup_en = 1;
    conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;
    conf.scl_pullup_en = 1;
    conf.master.clk_speed = 100000;
    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode, 0, 0 ,0));
    ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));
    return ESP_OK;
}

void debug_function_name(const char * name) 
{
}

void debug_last_task(char * task_name) 
{
}

void debug_last_out_task(char * task_name) 
{
}

static void checkDevType(void) {
    i2cInit();
    pcf8574_init();
    int read_i2c_value = -1;
    read_i2c_value = ssd1306_WriteCommand(0xAE); //display off
    if (read_i2c_value == ESP_OK) {
        config.dev_type = T_DEV_TYPE_CLIENT;
    }
    else {
        config.dev_type = T_DEV_TYPE_SERVER;
    }
}

void app_main()
{
    configInit();
    checkDevType();
    
    if (config.dev_type != T_DEV_TYPE_SERVER)
    {
        battery_init();
        osDelay(10);

        // Inicjalizacja diod
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1 << MOTOR_LED_RED) | (1 << MOTOR_LED_GREEN) | (1 << SERVO_VIBRO_LED_RED) | (1 << SERVO_VIBRO_LED_GREEN);
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 1;
        gpio_config(&io_conf);

        buzzer_init();
        power_on_init();

        /* Wait to measure voltage */
        while(!battery_is_measured())
        {
            osDelay(10);
        }
        float voltage = battery_get_voltage();

        ssd1306_Init();
        power_on_enable_system();
        init_buttons();

        if (voltage > 3.2)
        {
            init_menu(MENU_DRV_NORMAL_INIT);
            wifiDrvInit();
            keepAliveStartTask();
            menuParamInit();
            fastProcessStartTask();
            // init_sleep();
        }
        else
        {
            init_menu(MENU_DRV_LOW_BATTERY_INIT);
        }
    }
    else {

        wifiDrvInit();
        keepAliveStartTask();
        menuParamInit();

        #if CONFIG_DEVICE_SOLARKA
        vibro_init();
        #endif
        #if CONFIG_DEVICE_SIEWNIK
        measure_start();
        #endif
        srvrControllStart();
        ultrasonar_start();
        //WYLACZONE
        errorSiewnikStart();
        //LED on
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1 << blink_pin) | (1 << 23);
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);
        gpio_set_level(23, 1);
    }

    config_printf(PRINT_DEBUG, PRINT_DEBUG, "[MENU] ------------START SYSTEM-------------");
    while(1)
    {
        vTaskDelay(MS2ST(975));
        // if (config.dev_type == T_DEV_TYPE_SERVER)
        // {
        //    gpio_set_level(blink_pin, 0);
        // }

        vTaskDelay(MS2ST(25));

        // if (config.dev_type == T_DEV_TYPE_SERVER)
        // {
        //    gpio_set_level(blink_pin, 1);
        // }
    }
}
