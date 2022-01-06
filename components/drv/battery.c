#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "config.h"


//max ADC: 375 - 4.2 V

#define MIN_ADC 290
#define MAX_ADC_FOR_MAX_VOL 375
#define MIN_VOL 3.2
#define MAX_VOL 4.2

#define ADC_BUFFOR_SIZE 32
static uint16_t adc_filtered_value;
static float voltage;
static float voltage_sum;
static float voltage_average;
static uint16_t adc_data[ADC_BUFFOR_SIZE];
static float voltage_table[ADC_BUFFOR_SIZE];
static uint32_t voltage_measure_cnt;
static uint32_t voltage_table_size;

static void adc_task()
{
    int x;
	int ret;
    while (1) {
		ret = adc_read_fast(adc_data, ADC_BUFFOR_SIZE);
        if (ESP_OK == ret) 
		{
			adc_filtered_value = 0;
            for (x = 0; x < ADC_BUFFOR_SIZE; x++) {
				//debug_msg("A: %d\n\r", adc_data[x]);
                adc_filtered_value = adc_filtered_value + adc_data[x];
            }
			adc_filtered_value = adc_filtered_value/ADC_BUFFOR_SIZE;
            voltage = MAX_VOL * adc_filtered_value / MAX_ADC_FOR_MAX_VOL;

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

        }
		else 
		{
			debug_msg("ADC error: %d\n\r", ret);
		}
		//debug_msg("Average: %d measured %d\n\r", (int)(voltage_average * 1000), (int)(voltage * 1000));
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

float battery_get_voltage(void) {
    return voltage_average;
}

void battery_init(void)
{
    // 1. init adc
    adc_config_t adc_config;

    // Depend on menuconfig->Component config->PHY->vdd33_const value
    // When measuring system voltage(ADC_READ_VDD_MODE), vdd33_const must be set to 255.
    adc_config.mode = ADC_READ_TOUT_MODE;
    adc_config.clk_div = 8; // ADC sample collection clock = 80MHz/clk_div = 10MHz
    ESP_ERROR_CHECK(adc_init(&adc_config));

    // 2. Create a adc task to read adc value
    xTaskCreate(adc_task, "adc_task", 1024, NULL, 5, NULL);
}
