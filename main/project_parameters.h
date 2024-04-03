/**
 *******************************************************************************
 * @file    parameters.h
 * @author  Dmytro Shevchenko
 * @brief   Parameters for working controller
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _PROJECT_PARAMETERS_H
#define _PROJECT_PARAMETERS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"

/* Public types --------------------------------------------------------------*/

/* PARAM(param, min_value, max_value, default_value, name) */

#define PARAMETERS_U32_LIST                           \
  PARAM( PARAM_MOTOR, 0, 100, 0 )                     \
  PARAM( PARAM_MOTOR2, 0, 100, 0 )                    \
  PARAM( PARAM_SERVO, 0, 100, 0 )                     \
  PARAM( PARAM_VIBRO_ON_S, 0, 100, 0 )                \
  PARAM( PARAM_VIBRO_OFF_S, 0, 100, 0 )               \
  PARAM( PARAM_MOTOR_IS_ON, 0, 1, 0 )                 \
  PARAM( PARAM_SERVO_IS_ON, 0, 1, 0 )                 \
  PARAM( PARAM_VOLTAGE_SERVO, 0, 0xFFFF, 0 )          \
  PARAM( PARAM_CURRENT_MOTOR, 0, 0xFFFF, 0 )          \
  PARAM( PARAM_VOLTAGE_ACCUM, 0, 0xFFFF, 0 )          \
  PARAM( PARAM_TEMPERATURE, 0, 0xFFFF, 0 )            \
  PARAM( PARAM_SILOS_LEVEL, 0, 100, 0 )               \
  PARAM( PARAM_SILOS_HEIGHT, 0, 300, 60 )             \
  PARAM( PARAM_START_SYSTEM, 0, 1, 0 )                \
  PARAM( PARAM_LOW_LEVEL_SILOS, 0, 1, 0 )             \
  PARAM( PARAM_SILOS_SENSOR_IS_CONNECTED, 0, 1, 0 )   \
  PARAM( PARAM_LANGUAGE, 0, 3, 0 )                    \
  PARAM( PARAM_PERIOD, 0, 180, 10 )                   \
                                                      \
  PARAM( PARAM_MACHINE_ERRORS, 0, 0xFFFF, 0 )         \
                                                      \
  PARAM( PARAM_ERROR_SERVO, 0, 1, 1 )                 \
  PARAM( PARAM_ERROR_MOTOR, 0, 1, 1 )                 \
  PARAM( PARAM_ERROR_SERVO_CALIBRATION, 0, 99, 20 )   \
  PARAM( PARAM_ERROR_MOTOR_CALIBRATION, 0, 99, 50 )   \
  PARAM( PARAM_MOTOR_MIN_CALIBRATION, 0, 100, 20 )    \
  PARAM( PARAM_MOTOR_MAX_CALIBRATION, 0, 100, 100 )   \
  PARAM( PARAM_CLOSE_SERVO_REGULATION_FLAG, 0, 1, 0 ) \
  PARAM( PARAM_OPEN_SERVO_REGULATION_FLAG, 0, 1, 0 )  \
  PARAM( PARAM_CLOSE_SERVO_REGULATION, 0, 99, 50 )    \
  PARAM( PARAM_OPEN_SERVO_REGULATION, 0, 99, 50 )     \
  PARAM( PARAM_TRY_OPEN_CALIBRATION, 0, 10, 8 )

#endif