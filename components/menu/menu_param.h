#ifndef MENU_PARAM_H
#define MENU_PARAM_H

#include "config.h"

typedef enum
{
	MENU_MOTOR,
	MENU_MOTOR2,
	MENU_SERVO,
	MENU_VIBRO_PERIOD,
	MENU_VIBRO_WORKING_TIME,
	MENU_MOTOR_IS_ON,
	MENU_SERVO_IS_ON,
	MENU_CURRENT_SERVO,
	MENU_CURRENT_MOTOR,
	MENU_VOLTAGE_ACCUM,
	MENU_TEMPERATURE,
	MENU_SILOS_LEVEL,
	MENU_START_SYSTEM,
	MENU_BOOTUP_SYSTEM,
	MENU_EMERGENCY_DISABLE,
	MENU_LOW_LEVEL_SILOS,

	/* ERRORS */
	MENU_MOTOR_ERROR_OVERCURRENT,
	MENU_SERVO_ERROR_OVERCURRENT,
	MENU_TEMPERATURE_IS_ERROR_ON,
	MENU_MOTOR_ERROR_NOT_CONNECTED,
	MENU_SERVO_ERROR_NOT_CONNECTED,
	MENU_SERVO_ERROR_CANOT_CLOSE,

	/* calibration value */
	MENU_ERROR_SERVO,
	MENU_ERROR_MOTOR,
	MENU_ERROR_SERVO_CALIBRATION,
	MENU_ERROR_MOTOR_CALIBRATION,
	MENU_MOTOR_MIN_CALIBRATION,
	MENU_MOTOR_MAX_CALIBRATION,
	MENU_BUZZER,
	MENU_CLOSE_SERVO_REGULATION_FLAG,
	MENU_OPEN_SERVO_REGULATION_FLAG,
	MENU_CLOSE_SERVO_REGULATION,
	MENU_OPEN_SERVO_REGULATION,
	MENU_TRY_OPEN_CALIBRATION,
	MENU_LAST_VALUE

}menuValue_t;

void menuParamInit(void);
esp_err_t menuSaveParameters(void);
esp_err_t menuReadParameters(void);
void menuSetDefaultValue(void);
uint32_t menuGetValue(menuValue_t val);
uint32_t menuGetMaxValue(menuValue_t val);
uint32_t menuGetDefaultValue(menuValue_t val);
uint8_t menuSetValue(menuValue_t val, uint32_t value);
void menuParamGetDataNSize(void ** data, uint32_t * size);
void menuParamSetDataNSize(void * data, uint32_t size);

void menuPrintParameters(void);
void menuPrintParameter(menuValue_t val);

#endif