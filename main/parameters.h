/**
 *******************************************************************************
 * @file    parameters.h
 * @author  Dmytro Shevchenko
 * @brief   Parameters for working controller
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _PARAMETERS_H
#define _PARAMETERS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"

/* Public types --------------------------------------------------------------*/

typedef void ( *param_set_cb )( void* user_data, uint32_t value );

typedef enum
{
  PARAM_MOTOR,
  PARAM_MOTOR2,
  PARAM_SERVO,
  PARAM_VIBRO_ON_S,
  PARAM_VIBRO_OFF_S,
  PARAM_MOTOR_IS_ON,
  PARAM_SERVO_IS_ON,
  PARAM_VOLTAGE_SERVO,
  PARAM_CURRENT_MOTOR,
  PARAM_VOLTAGE_ACCUM,
  PARAM_TEMPERATURE,
  PARAM_SILOS_LEVEL,
  PARAM_SILOS_SENSOR_IS_CONECTED,
  PARAM_SILOS_HEIGHT,
  PARAM_START_SYSTEM,
  PARAM_BOOT_UP_SYSTEM,
  PARAM_EMERGENCY_DISABLE,
  PARAM_LOW_LEVEL_SILOS,
  PARAM_LANGUAGE,
  PARAM_POWER_ON_MIN,
  PARAM_PERIOD,

  /* ERRORS */
  PARAM_MACHINE_ERRORS,

  /* calibration value */
  PARAM_ERROR_SERVO,
  PARAM_ERROR_MOTOR,
  PARAM_ERROR_SERVO_CALIBRATION,
  PARAM_ERROR_MOTOR_CALIBRATION,
  PARAM_MOTOR_MIN_CALIBRATION,
  PARAM_MOTOR_MAX_CALIBRATION,
  PARAM_BUZZER,
  PARAM_BRIGHTNESS,
  PARAM_CLOSE_SERVO_REGULATION_FLAG,
  PARAM_OPEN_SERVO_REGULATION_FLAG,
  PARAM_CLOSE_SERVO_REGULATION,
  PARAM_OPEN_SERVO_REGULATION,
  PARAM_TRY_OPEN_CALIBRATION,
  PARAM_LAST_VALUE

} parameter_value_t;

typedef struct
{
  uint32_t min_value;
  uint32_t max_value;
  uint32_t default_value;
  const char* name;

  void* user_data;
  param_set_cb cb;
} parameter_t;

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init parameters.
 */
void parameters_init( void );

/**
 * @brief   Save parameters in nvm.
 * @return  true - if success
 */
bool parameters_save( void );

/**
 * @brief   Set default values.
 */
void parameters_setDefaultValues( void );

/**
 * @brief   Get value.
 * @param   [in] val - parameter which get value
 * @return  value of parameter
 */
uint32_t parameters_getValue( parameter_value_t val );

/**
 * @brief   Get max value.
 * @param   [in] val - parameter which get value
 * @return  value of parameter
 */
uint32_t parameters_getMaxValue( parameter_value_t val );

/**
 * @brief   Get min value.
 * @param   [in] val - parameter which get value
 * @return  value of parameter
 */
uint32_t parameters_getMinValue( parameter_value_t val );

/**
 * @brief   Get default values.
 * @param   [in] val - parameter which get value
 * @return  value of parameter
 */
uint32_t parameters_getDefaultValue( parameter_value_t val );

/**
 * @brief   Set values.
 * @param   [in] val - parameter which set value
 * @return  true - if success
 */
bool parameters_setValue( parameter_value_t val, uint32_t value );

/**
 * @brief   Print in serial all parameters
 */
void parameters_debugPrint( void );

/**
 * @brief   Print in serial parameter value
 * @param   [in] val - parameter which print value
 */
void parameters_debugPrintValue( parameter_value_t val );

#endif