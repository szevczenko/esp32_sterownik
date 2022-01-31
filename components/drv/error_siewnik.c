#include <stdint.h>
#include "measure.h"
#include "error_siewnik.h"
#include "motor.h"
#include "menu.h"
#include "servo.h"
#include "math.h"
#include "menu_param.h"

#include "cmd_server.h"


void error_event(void * arg)
{
	//static uint32_t error_event_timer;
	while(1)
	{
		vTaskDelay(350 / portTICK_RATE_MS);
	} //error_event_timer
}


void error_led_blink(void)
{

}

#if 0
static void set_error_state(error_reason_ reason)
{
	if (reason == ERR_REASON_SERVO)
		cmdServerSetValueWithoutResp(MENU_SERVO_ERROR_IS_ON, 1);
	else {
		cmdServerSetValueWithoutResp(MENU_MOTOR_ERROR_IS_ON, 1);
		servo_error(1);
	}
	error_events = 1;
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
#endif

void errorSiewnikStart(void)
{
	xTaskCreate(error_event, "error_event", 748*2, NULL, NORMALPRIO, NULL);
}