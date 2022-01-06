#include "server_controller.h"
#include "menu_param.h"
#include "parse_cmd.h"
#include "motor.h"
#include "servo.h"

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

static void state_init(void)
{
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
	xTaskCreate(_task, "srvrController", 1024, NULL, 10, NULL);
}