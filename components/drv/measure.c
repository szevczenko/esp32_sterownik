#include <stdint.h>
#include "measure.h"
#include "config.h"
#include "atmega_communication.h"
#include "freertos/timers.h"
#include "menu_param.h"
#include "parse_cmd.h"
#include "cmd_server.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "measure.h"
#include "ultrasonar.h"

//#undef debug_msg
//#define debug_msg(...)

static adc_bits_width_t width = ADC_WIDTH_BIT_12;
static adc_atten_t atten = ADC_ATTEN_DB_11;
static uint32_t send_cnt;

#define ADC_IN_CH		ADC_CHANNEL_6
#define ADC_MOTOR_CH	ADC_CHANNEL_7
#define ADC_SERVO_CH	ADC_CHANNEL_4
#define ADC_12V_CH		ADC_CHANNEL_5
#define ADC_CE_CH		ADC_CHANNEL_5

#ifndef ADC_REFRES
#define ADC_REFRES 4096
#endif

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

//600
#define SERVO_CALIBRATION_VALUE calibration_value 

typedef struct 
{
	char * ch_name;
	adc_channel_t channel;
	adc_unit_t unit;
	uint32_t adc;
	uint32_t filtered_adc;
	uint32_t filter_table[FILTER_TABLE_SIZE];
	float meas_voltage;
}meas_data_t;

static meas_data_t meas_data[MEAS_CH_LAST] = 
{
	[MEAS_CH_IN] 	= {.unit = 1, .channel = ADC_IN_CH, .ch_name = "MEAS_CH_IN"},
	[MEAS_CH_MOTOR] = {.unit = 1, .channel = ADC_MOTOR_CH, .ch_name = "MEAS_CH_MOTOR"},
	[MEAS_CH_SERVO] = {.unit = 1, .channel = ADC_SERVO_CH, .ch_name = "MEAS_CH_SERVO"},
	[MEAS_CH_12V] 	= {.unit = 1, .channel = ADC_12V_CH, .ch_name = "MEAS_CH_12V"},
	[MEAS_CH_TEMP] 	= {.unit = 2, .channel = ADC_CE_CH, .ch_name = "MEAS_CH_TEMP"},
};

static uint32_t table_size;
static uint32_t table_iter;

static TimerHandle_t xTimers;

#if CONFIG_DEVICE_SIEWNIK
static uint32_t calibration_value;
static void measure_get_servo_calibration(TimerHandle_t xTimer)
{
	calibration_value = measure_get_filtered_value(MEAS_CH_SERVO);
	debug_msg("MEASURE SERVO Calibration value = %d\n", calibration_value);
}
#endif

static uint32_t filtered_value(uint32_t *tab, uint8_t size)
{
	uint16_t ret_val = *tab;
	for (uint8_t i = 1; i<size; i++)
	{
		ret_val = (ret_val + tab[i])/2;
	}
	return ret_val;
}

void init_measure(void)
{

}

static void _read_adc_values(void)
{
	for(uint8_t ch = 0; ch < MEAS_CH_LAST; ch++)
	{
		meas_data[ch].adc = 0;
		int ret_v = 0;
		//Multisampling
		for (int i = 0; i < NO_OF_SAMPLES; i++) 
		{
			if (meas_data[ch].unit == ADC_UNIT_1)
			{
				meas_data[ch].adc += adc1_get_raw((adc1_channel_t)meas_data[ch].channel);
			}
			else
			{
				int raw = 0;
                ret_v = adc2_get_raw((adc2_channel_t)meas_data[ch].channel, width, &raw);
                meas_data[ch].adc += raw;
			}
		}
		if (meas_data[ch].unit == ADC_UNIT_2)
		{
			//printf("Pomiar 2 %d\n\r", ret_v);
		}
		
		meas_data[ch].adc /= NO_OF_SAMPLES;
		meas_data[ch].filter_table[table_iter%FILTER_TABLE_SIZE] = meas_data[ch].adc;
		meas_data[ch].filtered_adc = filtered_value(&meas_data[ch].adc, table_size);
	}
	table_iter++;
	if (table_size < FILTER_TABLE_SIZE - 1)
	{
		table_size++;
	}
}

#define SILOS_START_MEASURE 230
#define SILOS_LOW 300

static void measure_process(void * arg)
{
	(void)arg;
	while(1) {
		vTaskDelay(100 / portTICK_RATE_MS);

		_read_adc_values();

		// #if CONFIG_DEVICE_SOLARKA
		// #endif
		// #if CONFIG_DEVICE_SIEWNIK
		// accum_adc += motor_filter_value*0.27; //motor_filter_value*0.0075*1025/5/5.7
		// #endif

		uint32_t silos_distance = ultrasonar_get_distance() > SILOS_START_MEASURE ? ultrasonar_get_distance() - SILOS_START_MEASURE : 0;
		int silos_percent = (SILOS_LOW - silos_distance) * 100 / SILOS_LOW;
		if (silos_percent < 0 || silos_percent > 100)
		{
			silos_percent = 0;
		}
		

		if (send_cnt % 30 == 0)
		{
			if (cmdServerSetValueWithoutResp(MENU_LOW_LEVEL_SILOS, silos_percent < 10) == 0)
			{
				printf("[MEAS] Can't send silos data\n\r");
			}
			printf("silos_distance %d silos_percent %d\n\r", silos_distance, silos_percent);
		}
		send_cnt++;
		
		menuSetValue(MENU_SILOS_LEVEL, (uint32_t)silos_percent);
		menuSetValue(MENU_VOLTAGE_ACCUM, (uint32_t)(accum_get_voltage() * 10000.0));
		menuSetValue(MENU_CURRENT_MOTOR, (uint32_t)(measure_get_current(MEAS_CH_MOTOR, 0.1) * 1000.0));
		menuSetValue(MENU_TEMPERATURE, (uint32_t)(measure_get_temperature() * 100.0));
		/* DEBUG */
		// menuPrintParameter(MENU_VOLTAGE_ACCUM);
		// menuPrintParameter(MENU_CURRENT_MOTOR);
		// menuPrintParameter(MENU_TEMPERATURE);
	}
}

void measure_start(void) 
{
	adc1_config_width(width);
    adc1_config_channel_atten(ADC_CHANNEL_4, atten);
	adc1_config_channel_atten(ADC_CHANNEL_5, atten);
	adc1_config_channel_atten(ADC_CHANNEL_6, atten);
	adc1_config_channel_atten(ADC_CHANNEL_7, atten);
	adc2_config_channel_atten((adc2_channel_t)ADC_CHANNEL_5, atten);

	xTaskCreate(measure_process, "measure_process", 4096, NULL, 10, NULL);
	#if CONFIG_DEVICE_SIEWNIK
	xTimers = xTimerCreate("at_com_tim", 2000 / portTICK_RATE_MS, pdFALSE, ( void * ) 0, measure_get_servo_calibration);
	xTimerStart( xTimers, 0 );
	#endif
}

uint32_t measure_get_filtered_value(enum_meas_ch type)
{
	if (type < MEAS_CH_LAST)
	{
		return meas_data[type].filtered_adc;
	}
	return 0;
}

float measure_get_temperature(void)
{
	return measure_get_filtered_value(MEAS_CH_TEMP);
}

float measure_get_current(enum_meas_ch type, float resistor)
{
	uint32_t adc = measure_get_filtered_value(type) < 1900 ? 0 : measure_get_filtered_value(type) - 1900;
	float volt = (float) adc / 23 /* Volt */;
	return volt;
}

float accum_get_voltage(void)
{
	float voltage = 0;
	#if CONFIG_DEVICE_SOLARKA
    voltage = measure_get_filtered_value(MEAS_CH_12V)*5*5.7/1024 + 0.7;
	#else
	voltage = (float)measure_get_filtered_value(MEAS_CH_12V) / 4096.0 / 2.6;
	#endif
    return voltage;
}