#include "server_controller.h"
#include "menu_param.h"
#include "parse_cmd.h"
#include "motor.h"
#include "servo.h"

#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

#define MOTOR_PWM_PIN 27
#define SERVO_PWM_PIN 26

typedef enum
{
	STATE_INIT,
	STATE_IDLE,
	STATE_WORKING,
	STATE_SERVO_REGULATION,
	STATE_MOTOR_REGULATION,
	STATE_EMERGENCY_DISABLE,
	STATE_LAST
}state_t;

typedef struct 
{
	state_t state;
	uint8_t servo_value;
	uint8_t motor_value;

	uint8_t motor_on;
	uint8_t servo_on;
	uint16_t servo_pwm;
	uint8_t motor_pwm;
	bool system_on;
	bool emergency_disable;

	bool working_state_req;
	bool motor_calibration_req;
	bool servo_calibration_req;
} server_conroller_ctx;

static server_conroller_ctx ctx;
static char * state_name[] =
{
	[STATE_INIT] = "STATE_INIT",
	[STATE_IDLE] = "STATE_IDLE",
	[STATE_WORKING] = "STATE_WORKING",
	[STATE_SERVO_REGULATION] = "STATE_SERVO_REGULATION",
	[STATE_MOTOR_REGULATION] = "STATE_MOTOR_REGULATION",
	[STATE_EMERGENCY_DISABLE] = "STATE_EMERGENCY_DISABLE"
};

static void change_state(state_t state)
{
	if (state >= STATE_LAST)
	{
		return;
	}
	
	if (state != ctx.state)
	{
		debug_msg("Change state -> %s\n\r", state_name[state])
		ctx.state = state;
	}
}

static void count_working_data(void)
{
	ctx.motor_pwm = dcmotor_process(ctx.motor_value);
	ctx.servo_pwm = servo_process(ctx.servo_on ? ctx.servo_value : 0);
	if (ctx.motor_on) {
		motor_start();
	}
	else {
		motor_stop();
	}
}

static void set_working_data(void)
{
	//#if CONFIG_DEVICE_SIEWNIK
	printf("motor %d %d\n\r", ctx.motor_on, ctx.motor_pwm);
	if (ctx.motor_on)
	{
		float duty = (float)ctx.motor_pwm * 100 / 255.0;
		printf("duty motor %f\n\r", duty);
		mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, duty);
		mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); //call this each time, if operator was previously in low/high state
	}
	else
	{
		mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
	}

	printf("servo %d %d\n\r", ctx.motor_on, ctx.motor_pwm);
	if (ctx.servo_on)
	{
		float duty = (float)ctx.servo_pwm * 100 / 19999.0;
		printf("duty servo %f\n\r", duty);
		mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A, duty);
		mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); //call this each time, if operator was previously in low/high state
	}
	else
	{
		mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A);
	}

	//#endif
}

static void state_init(void)
{
	mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, MOTOR_PWM_PIN);
	mcpwm_config_t pwm_config;
    pwm_config.frequency = 1000;    //frequency = 1000Hz
    pwm_config.cmpr_a = 0;       //duty cycle of PWMxA = 60.0%
    pwm_config.cmpr_b = 0;       	//duty cycle of PWMxb = 50.0%
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);   //Configure PWM0A & PWM0B with above settings

	mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, SERVO_PWM_PIN);
	pwm_config.frequency = 1;
    pwm_config.cmpr_a = 0;
    pwm_config.cmpr_b = 0;
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
	mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);   //Configure PWM0A & PWM0B with above settings

	change_state(STATE_IDLE);
}

static void state_idle(void)
{
	ctx.servo_value = 0;
	ctx.motor_value = 0;
	ctx.motor_on = false;
	ctx.servo_on = false;

	ctx.working_state_req = (bool)menuGetValue(MENU_START_SYSTEM);
	ctx.emergency_disable = (bool)menuGetValue(MENU_EMERGENCY_DISABLE);

	if (ctx.emergency_disable)
	{
		change_state(STATE_EMERGENCY_DISABLE);
		return;
	}

	if (ctx.working_state_req)
	{
		change_state(STATE_WORKING);
		return;
	}
	osDelay(100);
}

static void state_working(void)
{
	ctx.servo_value = (uint8_t)menuGetValue(MENU_SERVO);
	ctx.motor_value = (uint8_t)menuGetValue(MENU_MOTOR);
	ctx.motor_on = (uint16_t)menuGetValue(MENU_MOTOR_IS_ON);
	ctx.servo_on = menuGetValue(MENU_SERVO_IS_ON) > 0;

	ctx.working_state_req = (bool)menuGetValue(MENU_START_SYSTEM);
	ctx.emergency_disable = (bool)menuGetValue(MENU_EMERGENCY_DISABLE);

	#if CONFIG_DEVICE_SOLARKA
	vibro_config(menuGetValue(MENU_VIBRO_PERIOD) * 1000, menuGetValue(MENU_VIBRO_WORKING_TIME) * 1000);
	if (menuGetValue(MENU_SERVO_IS_ON)) {
		vibro_start();
	}
	else {
		vibro_stop();
	}
	data_write[AT_W_SERVO_IS_ON] = (uint16_t)vibro_is_on();
	#endif

	if (ctx.emergency_disable)
	{
		change_state(STATE_EMERGENCY_DISABLE);
		return;
	}

	if (!ctx.working_state_req)
	{
		change_state(STATE_IDLE);
		return;
	}
	osDelay(100);
}

static void state_servo_regulation(void)
{
	change_state(STATE_IDLE);
}

static void state_motor_regulation(void)
{
	change_state(STATE_IDLE);
}

static void state_emergency_disable(void)
{
	ctx.emergency_disable = (bool)menuGetValue(MENU_EMERGENCY_DISABLE);
	ctx.servo_value = 0;
	ctx.motor_value = 0;
	ctx.motor_on = false;
	ctx.servo_on = false;

	if (!ctx.emergency_disable)
	{
		change_state(STATE_IDLE);
		return;
	}
	osDelay(100);
}

static void _task(void * arg)
{
	while(1)
	{
		switch (ctx.state)
		{
			case STATE_INIT:
				state_init();
				break;

			case STATE_IDLE:
				state_idle();
				break;

			case STATE_WORKING:
				state_working();
				break;

			case STATE_SERVO_REGULATION:
				state_servo_regulation();
				break;

			case STATE_MOTOR_REGULATION:
				state_motor_regulation();
				break;

			case STATE_EMERGENCY_DISABLE:
				state_emergency_disable();
				break;
		
			default:
			change_state(STATE_IDLE);
				break;
		}

		count_working_data();
		set_working_data();
	}
}

bool srvrControllGetMotorStatus(void)
{
	return ctx.motor_on;
}

bool srvrControllGetServoStatus(void)
{
	return ctx.servo_on;
}

uint8_t srvrControllGetMotorPwm(void)
{
	return ctx.motor_pwm;
}

uint16_t srvrControllGetServoPwm(void)
{
	return ctx.servo_pwm;
}

bool srvrControllGetEmergencyDisable(void)
{
	return ctx.emergency_disable;
}

void srvrControllStart(void)
{
	xTaskCreate(_task, "srvrController", 4096, NULL, 10, NULL);
}