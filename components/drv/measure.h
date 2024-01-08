#ifndef _MEASURE_H
#define _MEASURE_H

#include "config.h"

#define FILTER_TABLE_SIZE   8
#define FILTER_TABLE_S_SIZE 10

#define MOTOR_ADC_CH 2
#define SERVO_ADC_CH 1    //1

typedef enum
{
  MEAS_MOTOR,
  MEAS_SERVO,
  MEAS_TEMPERATURE,
  MEAS_ACCUM
} _type_measure;

typedef enum
{
  MEAS_CH_IN,
  MEAS_CH_MOTOR,
  MEAS_CH_12V,
  MEAS_CH_TEMP,
#if CONFIG_DEVICE_SIEWNIK
  MEAS_CH_SERVO,
  MEAS_CH_CHECK_SERVO,
  MEAS_CH_CHECK_MOTOR,
#endif
#if CONFIG_DEVICE_SOLARKA
  MEAS_CH_CHECK_VIBRO,
  MEAS_CH_CHECK_MOTOR,
#endif
  MEAS_CH_LAST
} enum_meas_ch;

void init_measure( void );
void measure_start( void );
void measure_meas_calibration_value( void );
uint32_t measure_get_filtered_value( enum_meas_ch type );
float measure_get_current( enum_meas_ch type, float resistor );
float accum_get_voltage( void );
float measure_get_temperature( void );
float measure_get_servo_voltage( void );

#endif