/*
 * pwm.c
 *
 * Created: 07.02.2019 13:19:09
 *  Author: Demetriusz
 */ 
#include <config.h>
#include <stdio.h>

#include "servo.h"
#include "menu_param.h"
//#include "error_siewnik.h"

#define LED_SERVO_OFF
#define LED_SERVO_ON 
#define OFF_SERVO

//#undef printf
//#define printf(...)

#if CONFIG_DEVICE_SIEWNIK

static void servo_exit_try(void);

sDriver servoD;
static uint8_t try_count = 0;

void servo_set_pwm_val(uint8_t value)
{
	int min = 2000 + (50 - menuGetValue(MENU_CLOSE_SERVO_REGULATION))*10;
	int max = 1275 + (50 - menuGetValue(MENU_OPEN_SERVO_REGULATION))*10;
	uint16_t pwm = (uint16_t)((float)(max-min)*(float)value/(float)99 + (float)min);
	//printf("REG: close %d, open %d, pwm %d\n", min, max, pwm);
	servoD.pwm_value = pwm;
	
	/*if (value == 0)
	set_pwm(2000);
	else if(value < 50)
	set_pwm(1800 - (value - 10)*7);
	else if(value <= 99)
	set_pwm(1520 - (value - 50)*5);*/
}

void servo_regulation(uint8_t value)
{
	servoD.state = SERVO_REGULATION;
	servo_set_pwm_val(value);
}

void servo_error(uint8_t close)
{
	if (close)
		servo_set_pwm_val(0);
	LED_SERVO_OFF;
	servoD.state = SERVO_ERROR_PROCESS;
}

static void servo_error_process(void)
{
	static TickType_t timeout;
	if (timeout != 0)
	{
		if (timeout < xTaskGetTickCount())
		{
			servoD.state = SERVO_ERROR;
			OFF_SERVO;
		}
	}
	else
	{
		timeout = xTaskGetTickCount() + MS2ST(3500);
	}
}


void servo_init(uint8_t prescaler)
{
	(void) prescaler;
	LED_SERVO_OFF;

	servo_set_pwm_val((uint16_t)0);
	servoD.state = SERVO_CLOSE;
	servoD.value = 0;
	//evTime_init(&servoD.timeout);
	servoD.try_cnt = 0;
	try_count = 0;
	printf("SERVO: init\n");
}

int servo_is_open(void)
{
	return servoD.state == SERVO_OPEN;
}

int servo_open(uint8_t value) // value - 0-100%
{
	if (servoD.state == SERVO_CLOSE || servoD.state == SERVO_OPEN)
	{
		servoD.state = SERVO_OPEN;
		servoD.value = value;
		servo_set_pwm_val((uint16_t)value);
		printf("SERVO_OPPENED %d\n", value);
		LED_SERVO_ON;
		//error_servo_timer();
		return 1;
	}
	else if (servoD.state == SERVO_TRY)
	{
		servo_exit_try();
		//error_servo_timer();
		return 1;
	}
	else return 0;
}

void servo_enable_try(void)
{
	if (servoD.state == SERVO_OPEN || servoD.state == SERVO_CLOSE)
	{
		servoD.last_state = servoD.state;
		servoD.state = SERVO_TRY;
	}
}

int servo_get_try_cnt(void)
{
	return servoD.try_cnt;
}

int servo_close(void)
{
	if (servo_is_open())
	{
		servo_set_pwm_val((uint16_t)0);
		servoD.state = SERVO_CLOSE;
		servoD.value = 0;
		printf("SERVO_CLOSED %d\n", servoD.value);
		LED_SERVO_OFF;
		//error_servo_timer();
		return 1;
	}
	else if (servoD.state == SERVO_TRY)
	{
		servo_exit_try();
		//error_servo_timer();
		return 1;
	}
	return 0;
}


void servo_try_reset_timeout(uint32_t time_ms)
{
	servoD.timeout = xTaskGetTickCount() + MS2ST(time_ms);
}

static void servo_try_process(void)
{
	static TickType_t timeout;
	if (try_count == 0)
	{
		timeout = xTaskGetTickCount() + MS2ST(250);
		try_count++;
		servo_set_pwm_val(servoD.value + try_count);
	}
	else if (try_count > 0 && try_count < TRY_OPEN_VAL)
	{
		if (timeout < xTaskGetTickCount() && timeout != 0)
		{
			timeout = xTaskGetTickCount() + MS2ST(250);
			try_count++;
			servo_set_pwm_val(servoD.value + try_count * 8 /*dark_menu_get_value(MENU_TRY_OPEN_CALIBRATION) */);
		}
	}
	else
	{
		timeout = 0;
		try_count = 0;
		servo_set_pwm_val(servoD.value);
		servoD.state = servoD.last_state;
		servoD.try_cnt++;
	}
	printf("SERVO_TRY %d\n", servoD.value + try_count);

}

static void servo_exit_try(void)
{
	if (servoD.last_state == SERVO_OPEN)
	{
		servoD.state = SERVO_OPEN;
		servo_close();
	}
	else if ((servoD.last_state == SERVO_CLOSE))
	{
		servoD.state = SERVO_OPEN;
		servo_open(servoD.value);
	}
	else return;
	try_count = 0;
	servoD.try_cnt++;
}

uint16_t servo_process(uint8_t value)
{
	//servo_set_pwm_val(value);
	//#if 0
	if (value == 0 && servoD.value != 0) {
		servo_close();
	}

	if (value != 0 && value != servoD.value)
	{
		servo_open(value);
	}

	switch(servoD.state)
	{
		case SERVO_OPEN:
		servoD.value = value;
		servo_set_pwm_val((uint16_t)value);
		break;

		case SERVO_CLOSE:
		servoD.value = 0;
		servo_set_pwm_val(0);
		break;

		case SERVO_TRY:
			servo_try_process();
			return servoD.value + try_count * 8;
		break;

		case SERVO_ERROR_PROCESS:
		servo_error_process();
		break;

		case SERVO_ERROR:
		OFF_SERVO;
		break;
	}
	if (servoD.timeout < xTaskGetTickCount() && servoD.timeout != 0) 
	{
		servoD.try_cnt = 0;
		servoD.timeout = 0;
		printf("SERVO: Zero try cnt\n");
	}
	//#endif
	return servoD.pwm_value;
}

#endif //CONFIG_DEVICE_SIEWNIK