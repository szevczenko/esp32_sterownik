#ifndef _MEASURE_H
#define _MEASURE_H

#include "config.h"

#define ACCUMULATOR_SIZE_TAB 20
#define ACCUMULATOR_ADC_CH 0 //0.028 [V/adc]
#define ACCUMULATOR_HIGH_VOLTAGE 600
#define ACCUMULATOR_LOW_VOLTAGE 395
#define ACCUMULATOR_VERY_LOW_VOLTAGE 350

#define FILTER_TABLE_SIZE 15
#define FILTER_TABLE_S_SIZE 10

#define MOTOR_ADC_CH 2
#define SERVO_ADC_CH 1//1


typedef enum
{
    MEAS_MOTOR,
    MEAS_SERVO,
	MEAS_TEMPERATURE,
    MEAS_ACCUM
}_type_measure;

void init_measure(void);
void measure_start(void);
uint16_t measure_get_filtered_value(_type_measure type);
uint16_t measure_get_value(_type_measure type);
float measure_get_current(_type_measure type, float resistor);
float accum_get_voltage(void);
float measure_get_temperature(void);

#endif