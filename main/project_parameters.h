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

#define PARAMETERS_U32_LIST                                                          \
  PARAM( PARAM_MOTOR, 0, 100, 0, "motor" )                                           \
  PARAM( PARAM_MOTOR2, 0, 100, 0, "motor_2" )                                        \
  PARAM( PARAM_SERVO, 0, 100, 0, "servo" )                                           \
  PARAM( PARAM_VIBRO_ON_S, 0, 100, 0, "vibro_on_s" )                                 \
  PARAM( PARAM_VIBRO_OFF_S, 0, 100, 0, "vibro_off_s" )                               \
  PARAM( PARAM_VIBRO_DUTY_PWM, 50, 100, 50, "vibro_duty_pwm" )                       \
  PARAM( PARAM_MOTOR_IS_ON, 0, 1, 0, "motor_is_on" )                                 \
  PARAM( PARAM_SERVO_IS_ON, 0, 1, 0, "servo_is_on" )                                 \
  PARAM( PARAM_VOLTAGE_SERVO, 0, 0xFFFF, 0, "voltage_servo" )                        \
  PARAM( PARAM_CURRENT_MOTOR, 0, 0xFFFF, 0, "current_motor" )                        \
  PARAM( PARAM_VOLTAGE_ACCUM, 0, 0xFFFF, 0, "voltage_accum" )                        \
  PARAM( PARAM_TEMPERATURE, 0, 0xFFFF, 0, "temperature" )                            \
  PARAM( PARAM_SILOS_LEVEL, 0, 100, 0, "silos_level" )                               \
  PARAM( PARAM_SILOS_HEIGHT, 0, 300, 60, "silos_height" )                            \
  PARAM( PARAM_START_SYSTEM, 0, 1, 0, "start_system" )                               \
  PARAM( PARAM_LOW_LEVEL_SILOS, 0, 1, 0, "low_level_silos" )                         \
  PARAM( PARAM_SILOS_SENSOR_IS_CONNECTED, 0, 1, 0, "silos_server_is_connected" )     \
  PARAM( PARAM_LANGUAGE, 0, 3, 0, "language" )                                       \
  PARAM( PARAM_PERIOD, 0, 180, 10, "period" )                                        \
                                                                                     \
  PARAM( PARAM_MACHINE_ERRORS, 0, 0xFFFF, 0, "machine_errors" )                      \
                                                                                     \
  PARAM( PARAM_ERROR_SERVO, 0, 1, 1, "error_servo" )                                 \
  PARAM( PARAM_ERROR_MOTOR, 0, 1, 1, "error_motor" )                                 \
  PARAM( PARAM_ERROR_SERVO_CALIBRATION, 0, 99, 20, "error_servo_calibration" )       \
  PARAM( PARAM_ERROR_MOTOR_CALIBRATION, 0, 99, 50, "error_motor_calibration" )       \
  PARAM( PARAM_MOTOR_MIN_CALIBRATION, 0, 100, 20, "motor_min_calibration" )          \
  PARAM( PARAM_MOTOR_MAX_CALIBRATION, 0, 100, 100, "motor_max_calibration" )         \
  PARAM( PARAM_CLOSE_SERVO_REGULATION_FLAG, 0, 1, 0, "close_servo_regulation_flag" ) \
  PARAM( PARAM_OPEN_SERVO_REGULATION_FLAG, 0, 1, 0, "open_servo_regulation_flag" )   \
  PARAM( PARAM_CLOSE_SERVO_REGULATION, 0, 99, 50, "close_servo_regulation" )         \
  PARAM( PARAM_OPEN_SERVO_REGULATION, 0, 99, 50, "open_servo_regulation" )           \
  PARAM( PARAM_TRY_OPEN_CALIBRATION, 0, 10, 8, "try_open_calibration" )              \
  PARAM( PARAM_SIZE_OF_GRAIN, 0, 2, 1, "size_of_grain" )                             \
  PARAM( PARAM_HIGH_OF_MACHINE, 0, 1000, 50, "hight_of_machine" )                    \
  PARAM( PARAM_VELOCITY, 0, 200, 45, "velocity" )                                    \
  PARAM( PARAM_WORK_AREA, 0, 100, 50, "work_area" )                                  \
  PARAM( PARAM_GRAIN_PER_HECTARE, 0, 1000, 50, "grain_per_hectare" )                 \
  PARAM( PARAM_AUTO_MODE, 0, 1, 1, "auto_mode" )

#endif