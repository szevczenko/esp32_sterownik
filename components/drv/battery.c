#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "power_on.h"

#define MODULE_NAME                       "[Battery] "
#define DEBUG_LVL                         PRINT_INFO

#if CONFIG_DEBUG_BATTERY
#define LOG(_lvl, ...)                          \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

#define DEFAULT_VREF        1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES       64          //Multisampling
#define CHARGER_STATUS_PIN  35

static esp_adc_cal_characteristics_t adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;


//max ADC: 375 - 4.2 V

#define MIN_ADC 290
#define MAX_ADC_FOR_MAX_VOL 2000
#define MIN_VOL 3200
#define CRITICAL_VOLTAGE 3000
#define MAX_VOL 4200

#define ADC_BUFFOR_SIZE 32
static uint32_t voltage;
static uint32_t voltage_sum;
static uint32_t voltage_average;
static uint32_t voltage_table[ADC_BUFFOR_SIZE];
static uint32_t voltage_measure_cnt;
static uint32_t voltage_table_size;
static bool voltage_is_measured;

static void adc_task()
{
    while (1) {
		uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, width, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV
        uint32_t voltage_meas = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
        LOG(PRINT_DEBUG, "Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);

        voltage = MAX_VOL * voltage_meas / MAX_ADC_FOR_MAX_VOL;

        voltage_table[voltage_measure_cnt % ADC_BUFFOR_SIZE] = voltage;
        voltage_measure_cnt++;
        if (voltage_table_size < ADC_BUFFOR_SIZE)
        {
            voltage_table_size++;
        }

        voltage_sum = 0;
        for(uint8_t i = 0; i < voltage_table_size; i++)
        {
            voltage_sum += voltage_table[i];
        }
        voltage_average = voltage_sum / voltage_table_size;

        voltage_is_measured = true;

        if (voltage < CRITICAL_VOLTAGE)
        {
            LOG(PRINT_INFO, "Found critical battery voltage. Power off");
            power_on_disable_system();
        }

		LOG(PRINT_DEBUG, "Average: %d measured %d", voltage_average, voltage );
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

bool battery_is_measured(void)
{
    return voltage_is_measured;
}

float battery_get_voltage(void) {
    return (float)voltage_average / 1000;
}

bool battery_get_charging_status(void)
{
    return gpio_get_level(CHARGER_STATUS_PIN) == 0;
}

void battery_init(void)
{
    // 1. init adc
    if (unit == ADC_UNIT_1) 
    {
        adc1_config_width(width);
        adc1_config_channel_atten(channel, atten);
    } 
    else 
    {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, &adc_chars);

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = 0;
	io_conf.pin_bit_mask = (1 << CHARGER_STATUS_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // 2. Create a adc task to read adc value
    xTaskCreate(adc_task, "adc_task", 4096, NULL, 5, NULL);
}
