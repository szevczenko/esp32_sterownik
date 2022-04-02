#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "config.h"

#include "power_on.h"
#include "driver/gpio.h"

#define POWER_HOLD_PIN 13

void power_on_init(void)
{
	gpio_config_t io_conf = {0};
	io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1 << POWER_HOLD_PIN);
    gpio_config(&io_conf);
}

void power_on_enable_system(void)
{
	gpio_set_level(POWER_HOLD_PIN, 1);
}

void power_on_disable_system(void)
{
	gpio_set_level(POWER_HOLD_PIN, 0);
}