#include <stdio.h>


#include "config.h"

#include "motor.h"
#include "menu.h"
#include "menu_param.h"

#undef debug_msg
#define debug_msg(...)

#define LED_MOTOR_OFF
#define LED_MOTOR_ON
#define CMD_MOTOR_OFF
#define CMD_MOTOR_ON
#define CMD_MOTOTR_SET_PWM(pwm)

/*
 * init a motor
 */
void motor_init(mDriver *motorD) {
	debug_msg("dcmotor init\n");
	motorD->state = MOTOR_OFF;
	LED_MOTOR_OFF;
	CMD_MOTOR_OFF;
}

void motor_deinit(mDriver *motorD)
{
	//debug_msg("dcmotor deinit\n");
	motorD->state = MOTOR_NO_INIT;
	CMD_MOTOR_OFF;
	LED_MOTOR_OFF;
}

/*
 * stop the motor
 */
int motor_stop(mDriver *motorD) {
	
	//set orc
	if (dcmotor_is_on(motorD))
	{
		debug_msg("dcmotor stop\n");
		CMD_MOTOR_OFF;
		LED_MOTOR_OFF;
		motorD->last_state = motorD->state;
		motorD->state = MOTOR_OFF;
		return 1;
	}
	else
	{
		 //debug_msg("dcmotor cannot stop\n");
	}
	return 0;
}

int dcmotor_is_on(mDriver *motorD)
{
	if (motorD->state == MOTOR_ON || motorD->state == MOTOR_AXELERATE || motorD->state == MOTOR_TRY)
	{
		return 1;
	} 
	else return 0;
}

int motor_start(mDriver *motorD)
{
	if (motorD->state == MOTOR_OFF)
	{
		//debug_msg("Motor Start\n");
		LED_MOTOR_ON;
		CMD_MOTOR_ON;
		motorD->last_state = motorD->state;
		motorD->state = MOTOR_AXELERATE;
		motorD->timeout = xTaskGetTickCount() + MS2ST(1000);
		return 1;
	}
	else 
	{
		//debug_msg("dcmotor canot start\n");
		return 0;
	}
}

int dcmotor_set_pwm(mDriver *motorD, float pwm)
{
	float pwm_set = 0;
	if (pwm > 100) {
		debug_msg("dcmotor_set_pwm > 100 %f\n\r", pwm);
		pwm = 100;
	}

	if (pwm < 0) {
		debug_msg("dcmotor_set_pwm < 0 %f\n\r", pwm);
		pwm = 0;
	}

	if (pwm == 0) {
		motorD->pwm_value = 0;
		return 1;
	}

	debug_msg("dcmotor_set_pwm %f\n", pwm);
	float min_value = (float)menuGetValue(MENU_MOTOR_MIN_CALIBRATION);
	float max_value = (float)menuGetValue(MENU_MOTOR_MAX_CALIBRATION);
	float range = 100.0;
	if (min_value > max_value) {
		debug_msg("dcmotor_set_pwm min_value > max_value\n\r");
		min_value = menuGetDefaultValue(MENU_MOTOR_MIN_CALIBRATION);
		max_value = menuGetDefaultValue(MENU_MOTOR_MAX_CALIBRATION);
	}
	pwm_set = (max_value - min_value) * pwm / range + min_value;
	motorD->pwm_value = pwm_set;
	CMD_MOTOTR_SET_PWM(count_pwm(motorD->pwm_value));
	return 1;
}

int dcmotor_get_pwm(mDriver *motorD)
{
	return motorD->pwm_value;
}

void dcmotor_set_error(mDriver *motorD)
{
	debug_msg("dcmotor error\n");
	motor_stop(motorD);
	motorD->state = MOTOR_ERROR;
}

int dcmotor_set_try(mDriver *motorD)
{
	if (dcmotor_is_on(motorD))
	{
		motorD->state = MOTOR_TRY;
		return 1;
	}
	return 0;
}

int dcmotor_set_normal_state(mDriver *motorD)
{
	if (dcmotor_is_on(motorD))
	{
		motorD->state = MOTOR_ON;
		return 1;
	}
	return 0;
}

void motor_regulation(mDriver *motorD, float pwm) {
	motorD->state = MOTOR_REGULATION;
	motorD->pwm_value = pwm;
}

float dcmotor_process(mDriver *motorD, uint8_t value)
{
	switch(motorD->state)
	{
		case MOTOR_ON:
		debug_msg("MOTOR_ON %d\n\r", value);
		dcmotor_set_pwm(motorD, (float)value);
		break;

		case MOTOR_OFF:
		debug_msg("MOTOR_OFF %d\n\r", value);
		motorD->pwm_value = 0;
		break;

		case MOTOR_TRY:
		debug_msg("MOTOR_TRY %d\n\r", value);
			if (value <= 50)
			{
				dcmotor_set_pwm(motorD, value + 20);
			}
			else if ((value > 50) && (value <= 70))
			{
				dcmotor_set_pwm(motorD, value + 15);
			}
			else
			{
				dcmotor_set_pwm(motorD, value);
			}
		break;

		case MOTOR_ERROR:
			debug_msg("MOTOR_ERROR %d\n\r", value);
			CMD_MOTOR_OFF;
		break;

		case MOTOR_AXELERATE:
			debug_msg("MOTOR_AXELERATE %d\n\r", value);
			motorD->state = MOTOR_ON; //!!
			break;					 //!
			dcmotor_set_pwm(motorD, 50);
			
			//debug_msg("MOTOR_AXELERATE %d\n", motorD->pwm_value);
			if (motorD->timeout < xTaskGetTickCount()) {
				motorD->state = MOTOR_ON;
			}

		break;
			
		case MOTOR_REGULATION:
			debug_msg("MOTOR_REGULATION %d\n\r", value);
			dcmotor_set_pwm(motorD, value);
		break;

		default:
			debug_msg("MOTOR_ERROR_STATE %d\n\r", motorD->state);
		break;
	}
		
	return motorD->pwm_value;
}

