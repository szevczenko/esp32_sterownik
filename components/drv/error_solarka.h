#ifndef _ERROR_SOLARKA_H_
#define _ERROR_SOLARKA_H_
#include "config.h"

#if CONFIG_DEVICE_SOLARKA

typedef enum
{
	ERROR_MOTOR_NOT_CONNECTED,
	ERROR_VIBRO_NOT_CONNECTED,
	ERROR_MOTOR_OVER_CURRENT,
	ERROR_VIBRO_OVER_CURRENT,
	ERROR_OVER_TEMPERATURE,
	ERROR_TOP
} error_type_t;

void error_event(void * arg);
void error_init(void);
void error_deinit(void);
void error_led_blink(void);

void error_servo_timer(void);

void errorSiewnikStart(void);
void errorSiewnikErrorReset(void);

#endif //#if CONFIG_DEVICE_SIEWNIK

#endif