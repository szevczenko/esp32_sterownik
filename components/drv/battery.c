#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;


//max ADC: 375 - 4.2 V

#define MIN_ADC 290
#define MAX_ADC_FOR_MAX_VOL 2000
#define MIN_VOL 3200
#define MAX_VOL 4200

#define ADC_BUFFOR_SIZE 32
static uint32_t voltage;
static uint32_t voltage_sum;
static uint32_t voltage_average;
static uint32_t voltage_table[ADC_BUFFOR_SIZE];
static uint32_t voltage_measure_cnt;
static uint32_t voltage_table_size;

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
        //printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);

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

		//printf("Average: %d measured %d\n\r", voltage_average, voltage );
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

float battery_get_voltage(void) {
    return (float)voltage_average / 1000;
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

    // 2. Create a adc task to read adc value
    xTaskCreate(adc_task, "adc_task", 4096, NULL, 5, NULL);
}
