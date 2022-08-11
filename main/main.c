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
#include "intf/i2c/ssd1306_i2c.h"
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
#include "error_solarka.h"
#include "oled.h"

uint16_t test_value;
static gpio_config_t io_conf;
static uint32_t blink_pin = GPIO_NUM_23;
portMUX_TYPE portMux = portMUX_INITIALIZER_UNLOCKED;

extern void ultrasonar_start(void);

void debug_function_name(const char *name)
{
}

void debug_last_task(char *task_name)
{
}

void debug_last_out_task(char *task_name)
{
}

void graphic_init(void)
{
    ssd1306_i2cInitEx(I2C_EXAMPLE_MASTER_SCL_IO, I2C_EXAMPLE_MASTER_SDA_IO, SSD1306_I2C_ADDR);
    sh1106_128x64_init();
    ssd1306_clearScreen();
    ssd1306_fillScreen(0x00);
    ssd1306_flipHorizontal(1);
    ssd1306_flipVertical(1);
    oled_init();

    // uint32_t counter = 0;
    // char print_buff[64] = {0};

    // while(1)
    // {
    //     sprintf(print_buff, "couner %d", counter++);
    //     oled_printFixed(2, 15, print_buff, STYLE_NORMAL);
    //     oled_printFixed(2, 24, "Test2", STYLE_NORMAL);
        
    //     osDelay(100);
    // }
}

static void checkDevType(void)
{
    graphic_init();
    pcf8574_init();
    int read_i2c_value = ESP_OK;
    // read_i2c_value = ssd1306_WriteCommand(0xAE); //display off
    if (read_i2c_value == ESP_OK)
    {
        config.dev_type = T_DEV_TYPE_CLIENT;
    }
    else
    {
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
        io_conf.pin_bit_mask =
            (1 << MOTOR_LED_RED) | (1 << MOTOR_LED_GREEN) | (1 << SERVO_VIBRO_LED_RED) | (1 << SERVO_VIBRO_LED_GREEN);
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 1;
        gpio_config(&io_conf);

        buzzer_init();
        power_on_init();

        /* Wait to measure voltage */
        while (!battery_is_measured())
        {
            osDelay(10);
        }

        float voltage = battery_get_voltage();

        // ssd1306_Init();
        power_on_enable_system();
        init_buttons();

        if (voltage > 3.2)
        {
            init_menu(MENU_DRV_NORMAL_INIT);
            wifiDrvInit();
            keepAliveStartTask();
            menuParamInit();
            fastProcessStartTask();
            power_on_start_task();
            // init_sleep();
        }
        else
        {
            init_menu(MENU_DRV_LOW_BATTERY_INIT);
            power_on_disable_system();
        }
    }
    else
    {
        wifiDrvInit();
        keepAliveStartTask();
        menuParamInit();

        //#if CONFIG_DEVICE_SIEWNIK
        measure_start();
        //#endif
        srvrControllStart();
        ultrasonar_start();
        //WYLACZONE

#if CONFIG_DEVICE_SIEWNIK
        errorSiewnikStart();
#endif

#if CONFIG_DEVICE_SOLARKA
        errorSolarkaStart();
#endif

        //LED on
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1 << blink_pin);
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);
        gpio_set_level(blink_pin, 1);
    }

    config_printf(PRINT_DEBUG, PRINT_DEBUG, "[MENU] ------------START SYSTEM-------------");
    while (1)
    {
        vTaskDelay(MS2ST(250));
        if ((config.dev_type == T_DEV_TYPE_SERVER) && !cmdServerIsWorking())
        {
            gpio_set_level(blink_pin, 0);
        }

        vTaskDelay(MS2ST(750));

        if (config.dev_type == T_DEV_TYPE_SERVER)
        {
            gpio_set_level(blink_pin, 1);
        }
    }
}
