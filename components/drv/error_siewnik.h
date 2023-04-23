#ifndef _ERROR_H_
#define _ERROR_H_
#include "config.h"

#if CONFIG_DEVICE_SIEWNIK
#define ERROR_CRITICAL_VOLTAGE 800
#define ERROR_M_TIME_EXIT 2000

#define SERVO_WAIT_TO_TRY 1500
#define SERVO_WAIT_AFTER_TRY 2000
#define SERVO_TRY_CNT 3

#define MOTOR_RESISTOR 0.033

typedef enum
{
	ERROR_MOTOR_NOT_CONNECTED,
	ERROR_VIBRO_NOT_CONNECTED,
	ERROR_MOTOR_OVER_CURRENT,
	ERROR_VIBRO_OVER_CURRENT,
	ERROR_OVER_TEMPERATURE,
	ERROR_SERVO_NOT_CONNECTED,
	ERROR_SERVO_OVER_CURRENT,
	ERROR_TOP
} error_type_t;

typedef enum
{
	ERR_M_OK,
	ERR_M_WAIT,
	ERR_M_AXELERATE,
	ERR_M_ERROR,
	ERR_M_EXIT
}err_motor_t;

typedef enum
{
	ERR_S_OK,
	ERR_S_WAIT,
	ERR_S_TRY,
	ERR_S_ERROR,
	/*ERR_S_EXIT */
}err_servo_t;

void error_event(void * arg);
float errorGetMotorVal(void);
void error_init(void);
void error_deinit(void);
void error_led_blink(void);

void error_servo_timer(void);

void errorSiewnikStart(void);
void errorSiewnikErrorReset(void);
void errorSiewnikServoChangeState(void);

#endif //#if CONFIG_DEVICE_SIEWNIK

#endif