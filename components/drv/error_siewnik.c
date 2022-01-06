#include <stdint.h>
#include "measure.h"
#include "error_siewnik.h"
#include "motor.h"
#include "menu.h"
#include "servo.h"
#include "math.h"
#include "menu_param.h"

#include "cmd_server.h"



#if CONFIG_DEVICE_SIEWNIK
///////////////////////////////////////
///TODO get_calibration_value
int get_calibration_value(uint8_t type);
int get_disp_value(uint8_t type);
static void set_error_state(error_reason_ reason);
static float count_motor_error_value(uint16_t x, float volt_accum);
static uint16_t count_motor_timeout_wait(uint16_t x);
static uint16_t count_motor_timeout_axelerate(uint16_t x);
static uint16_t count_servo_error_value(void);

error_reason_ led_blink;

extern uint32_t servo_vibro_value;
static uint8_t error_events;
//////////////////////////////////
//MOTOTR
extern uint32_t motor_value;
static uint32_t motor_timer;
static err_motor_t error_motor_state;
static err_motor_t error_motor_last_state;
static uint8_t error_motor_status;
static float motor_error_value;

//////////////////////////////////
//SERVO
static uint16_t servo_error_value;
static err_servo_t error_servo_state;
static uint8_t error_servo_status;
static uint32_t servo_timer;

void error_init(void)
{
	error_motor_state = ERR_M_OK;
	error_motor_last_state = ERR_M_OK;
	error_motor_status = 0;
	error_servo_status = 0;
	error_servo_state = ERR_S_OK;
	motor_timer = 0;
	servo_timer = 0;
	led_blink = ERR_REASON_NO;
	error_events = 0;
}

void error_deinit(void)
{
	error_motor_state = ERR_M_OK;
	error_motor_last_state = ERR_M_OK;
	error_motor_status = 0;
	error_servo_status = 0;
	error_servo_state = ERR_S_OK;
	motor_timer = 0;
	servo_timer = 0;
	led_blink = ERR_REASON_NO;
	error_events = 0;
}
 
void errorReset(void) {
	error_deinit();
	error_init();
	menuSetValue(MENU_MOTOR_ERROR_IS_ON, 0);
	menuSetValue(MENU_SERVO_ERROR_IS_ON, 0);
}

#define RESISTOR 1

float errorGetMotorVal(void)
{
	return motor_error_value;
}
static uint32_t error_servo_tim;
void error_servo_timer(void)
{
	debug_msg("ERROR: reset timer");
	error_servo_tim = xTaskGetTickCount() + MS2ST(2000);
}

uint8_t test_current = 11;

void error_event(void * arg)
{
	//static uint32_t error_event_timer;
	while(1)
	{
		vTaskDelay(350 / portTICK_RATE_MS);
		if (error_events == 1) continue;
		///////////////////////////////////////////////////////////////////////////////////////////
		//MOTOR
		//float volt = accum_get_voltage();
		motor_error_value = 10;//count_motor_error_value(dcmotor_get_pwm(), volt);
		//uint16_t motor_adc_filterd = measure_get_filtered_value(MEAS_MOTOR);
		float current = test_current;//measure_get_current(MEAS_MOTOR, MOTOR_RESISTOR);
		//debug_msg("MOTOR ADC: %d, current_max: %f, current: %f\n", motor_adc_filterd, motor_error_value, current);
		if (current > motor_error_value /* && dcmotor_is_on() */) //servo_vibro_value*5
		{
			error_motor_status = 1;
		}
		else
		{
			error_motor_status = 0;
		}
		if (menuGetValue(MENU_ERROR_MOTOR))
		{
			#if CONFIG_USE_ERROR_MOTOR
			if (error_motor_status == 1)
			{
				switch(error_motor_state)
				{
					case ERR_M_OK:
						error_motor_state = ERR_M_WAIT;
						debug_msg("ERROR STATUS: ERR_M_WAIT\n\r");
						motor_timer = xTaskGetTickCount() + MS2ST(count_motor_timeout_wait(dcmotor_get_pwm()));
					break;
					case ERR_M_WAIT:
					if (motor_timer != 0 && motor_timer < xTaskGetTickCount())
					{
						dcmotor_set_try();
						motor_timer = xTaskGetTickCount() + MS2ST(count_motor_timeout_axelerate(dcmotor_get_pwm()));
						error_motor_state = ERR_M_AXELERATE;
						debug_msg("ERROR STATUS: ERR_M_AXELERATE\n\r");
						//test_current = 1;
					}
					break;
					case ERR_M_AXELERATE:
						if (motor_timer != 0 && motor_timer < xTaskGetTickCount())
						{
							error_motor_state = ERR_M_ERROR;
							motor_timer = 0;
							debug_msg("ERROR STATUS: ERR_M_ERROR\n\r");
						}
					break;
					case ERR_M_ERROR:
						set_error_state(ERR_REASON_MOTOR);
					break;
					case ERR_M_EXIT:
						if (motor_timer != 0 && motor_timer < xTaskGetTickCount())
						{
							motor_timer = xTaskGetTickCount() + MS2ST(ERROR_M_TIME_EXIT);
							error_motor_state = error_motor_last_state;
							debug_msg("ERROR STATUS: go to last before wait\n\r");
						}
					break;
				}
			}
			else
			{
				//toDo
				switch(error_motor_state)
				{
					case ERR_M_OK:
					motor_timer = 0;
					break;
					case ERR_M_WAIT:
					if (motor_timer != 0 && motor_timer < xTaskGetTickCount())
					{
						motor_timer = MS2ST(ERROR_M_TIME_EXIT) + xTaskGetTickCount();
						error_motor_state = ERR_M_EXIT;
						error_motor_last_state = ERR_M_WAIT;
						debug_msg("ERROR STATUS: ERR_M_EXIT\n\r");
					}
					break;
					case ERR_M_AXELERATE:
					if (motor_timer != 0 && motor_timer < xTaskGetTickCount())
					{
						motor_timer = MS2ST(ERROR_M_TIME_EXIT) + xTaskGetTickCount();
						dcmotor_set_normal_state();
						error_motor_state = ERR_M_EXIT;
						error_motor_last_state = ERR_M_AXELERATE;
						debug_msg("ERROR STATUS: ERR_M_EXIT\n\r");
					}
					break;
					case ERR_M_ERROR:
						set_error_state(ERR_REASON_MOTOR);
						motor_timer = 0;
					break;
					case ERR_M_EXIT:
					if (motor_timer != 0 && motor_timer < xTaskGetTickCount())
					{
						debug_msg("ERROR STATUS: ERR_M_OK\n\r");
						error_motor_state = ERR_M_OK;
						motor_timer = 0;
					}
					break;
				}
			}
			#endif
		} /* Disable error */
		//////////////////////////////////////////////////////////////////////////////////////
		// SERVO
		if (menuGetValue(MENU_ERROR_SERVO))
		{
			#if CONFIG_USE_ERROR_SERVO
			servo_error_value = count_servo_error_value();
			uint16_t servo_filt_val = measure_get_filtered_value(MEAS_SERVO);
			//debug_msg("servo_error_value: %d, filtered value: %d\n", servo_error_value, servo_filt_val);
			if (servo_filt_val > servo_error_value && error_servo_tim < xTaskGetTickCount()) //servo_filt_val*5
			{
				//debug_msg("servo_error_value: %d\n", servo_error_value);
				error_servo_status = 1;
			}
			else
			{
				error_servo_status = 0;
			}
		
			if (error_servo_status == 1)
			{
				switch(error_servo_state)
				{
					case ERR_S_OK:
						error_servo_state = ERR_S_WAIT;
						debug_msg("ERROR STATUS: ERR_S_WAIT\n\r");
						servo_timer = MS2ST(SERVO_WAIT_TO_TRY) + xTaskGetTickCount();
					break;
					case ERR_S_WAIT:
						if (servo_timer < xTaskGetTickCount() && servo_timer != 0)
						{
							if (servo_get_try_cnt() > SERVO_TRY_CNT)
							{
								error_servo_state = ERR_S_ERROR;
								break;
							}
							servo_timer = MS2ST(SERVO_WAIT_AFTER_TRY) + xTaskGetTickCount();
							error_servo_state = ERR_S_TRY;
							servo_enable_try();
							debug_msg("ERROR STATUS: ERR_S_TRY\n\r");
						}
					break;
					case ERR_S_TRY:
						if (servo_timer < xTaskGetTickCount() && servo_timer != 0)
						{
							error_servo_state = ERR_S_OK;
							servo_timer = 0;
						}
					break;
					case ERR_S_ERROR:
						set_error_state(ERR_REASON_SERVO);
						servo_timer = 0;
					break;
				} //switch
			}// if (error_servo_status == 1)
			else
			{
				//toDo
				switch(error_servo_state)
				{
					case ERR_S_OK:
					break;
					case ERR_S_WAIT:
					if (servo_timer < xTaskGetTickCount() && servo_timer != 0)
					{
						servo_timer = MS2ST(ERROR_M_TIME_EXIT) + xTaskGetTickCount();
						error_servo_state = ERR_M_OK;
						debug_msg("ERROR STATUS: ERR_S_OK\n\r");
					}
					break;
					case ERR_S_TRY:
					if (servo_timer < xTaskGetTickCount() && servo_timer != 0)
					{
						error_servo_state = ERR_S_OK;
						servo_timer = 0;
						debug_msg("ERROR STATUS: ERR_S_OK\n\r");
						servo_try_reset_timeout(3500);
					}
					break;
					break;
					case ERR_S_ERROR:
						set_error_state(ERR_REASON_SERVO);
						servo_timer = 0;
					break;
				} //switch
			} //else (error_servo_status == 1)
			#endif
		} /* Disable error */
	} //error_event_timer
}


void error_led_blink(void)
{
	// static evTime blink_timer;
	// if (evTime_process_period(&blink_timer, 350))
	// {
	// 	if (led_blink == ERR_REASON_MOTOR)
	// 	{
	// 		LED_MOTOR_TOGGLE;
	// 		ON_BUZZ_SIGNAL;
	// 	}
	// 	else if (led_blink == ERR_REASON_SERVO)
	// 	{
	// 		LED_SERVO_TOGGLE;
	// 		ON_BUZZ_SIGNAL;
	// 	}
	// }
}

static void set_error_state(error_reason_ reason)
{
	if (reason == ERR_REASON_SERVO)
		cmdServerSetValueWithoutResp(MENU_SERVO_ERROR_IS_ON, 1);
	else {
		cmdServerSetValueWithoutResp(MENU_MOTOR_ERROR_IS_ON, 1);
		servo_error(1);
	}
	error_events = 1;
	// SET_PIN(system_events, EV_SYSTEM_ERROR_MOTOR);
	// display_set_error(reason);
	// dcmotor_set_error();
	// if (reason == ERR_REASON_MOTOR)
	// 	servo_error(1);
	// else servo_error(0);
	// system_error();
	// led_blink = reason;
}

int get_calibration_value(uint8_t type)
{
	return 100;
}

#define REZYSTANCJA_WIRNIKA 3

static float count_motor_error_value(uint16_t x, float volt_accum)
{
	float volt_in_motor = volt_accum * x/100;
	float volt_in_motor_nominal = 14.2 * x/100;
	float temp = 0.011*pow(x, 1.6281) + (volt_in_motor - volt_in_motor_nominal)/REZYSTANCJA_WIRNIKA;
	#if DARK_MENU
	temp += (float)(dark_menu_get_value(MENU_ERROR_MOTOR_CALIBRATION) - 50) * x/400;
	#endif
	/* Jak chcesz dobrac parametry mozesz dla testu odkomentowac linijke nizej debug_msg()
		Funkcja zwraca prad maksymalny
		x						- wartosc na wyswietlaczu PWM
		volt					- napiecie akumulatora
		0.011*pow(x, 1.6281)	- zalezosc wedlug twoich pomiarow wyznaczona w excel
		volt_in_motor			- napiecie podawane na silnik obecnie przeskalowane wedlug PWM
		volt_in_motor_nominal	- napiecie przy ktorym wykonane pomiary (14,2 V) przeskalowane wedlug PWM
		*/
	
	//debug_msg("CURRENT volt_in: %d, volt_nominal: %f, out_value: %f\n", volt_in_motor, volt_in_motor_nominal, temp);
	return temp;
}

static uint16_t count_motor_timeout_wait(uint16_t x)
{
	uint16_t timeout = 5000 - x*30;
	debug_msg("count_motor_timeout_wait: %d\n\r", timeout);
	return timeout; //5000[ms] - pwm*30
}

static uint16_t count_motor_timeout_axelerate(uint16_t x)
{
	uint16_t timeout = 5000 - x*30;
	debug_msg("count_motor_timeout_axelerate: %d\n\r", timeout);
	return timeout; //5000[ms] - pwm*30
}

static uint16_t count_servo_error_value(void)
{
	#if DARK_MENU
	int ret = dark_menu_get_value(MENU_ERROR_SERVO_CALIBRATION);
	if (ret < 0) {
		return 0;
	}
	else {
		return ret;
	}
	#else
	return 20;
	#endif
}

void errorSiewnikStart(void)
{
	xTaskCreate(error_event, "error_event", 748*2, NULL, NORMALPRIO, NULL);
}
#endif