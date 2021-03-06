#include <stdbool.h>
#include "server_controller.h"
#include "menu_param.h"
#include "parse_cmd.h"
#include "motor.h"
#include "servo.h"
#include "cmd_server.h"

#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"
#include "error_siewnik.h"
#include "measure.h"

#define MODULE_NAME                       "[Srvr Ctrl] "
#define DEBUG_LVL                         PRINT_INFO

#if CONFIG_DEBUG_SERVER_CONTROLLER
#define LOG(_lvl, ...)                          \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

#define MOTOR_PWM_PIN 27
#define SERVO_PWM_PIN 26
#define MOTOR_PWM_PIN2 25

typedef enum
{
	STATE_INIT,
	STATE_IDLE,
	STATE_WORKING,
	STATE_SERVO_OPEN_REGULATION,
	STATE_SERVO_CLOSE_REGULATION,
	STATE_MOTOR_REGULATION,
	STATE_EMERGENCY_DISABLE,
	STATE_ERROR,
	STATE_LAST
}state_t;

typedef struct 
{
	state_t state;
	mDriver motorD1;
	mDriver motorD2;
	uint8_t servo_value;
	uint8_t servo_new_value;
	uint8_t servo_set_value;
	uint32_t servo_set_timer;
	uint8_t motor_value;

	uint8_t motor_on;
	uint8_t servo_on;
	uint16_t servo_pwm;
	float motor_pwm;
	float motor_pwm2;
	bool system_on;
	bool emergency_disable;
	bool errors;

	bool working_state_req;
	bool motor_calibration_req;
	bool servo_open_calibration_req;
	bool servo_close_calibration_req;
} server_conroller_ctx;

static server_conroller_ctx ctx;
static char * state_name[] =
{
	[STATE_INIT] = "STATE_INIT",
	[STATE_IDLE] = "STATE_IDLE",
	[STATE_WORKING] = "STATE_WORKING",
	[STATE_SERVO_OPEN_REGULATION] = "STATE_SERVO_OPEN_REGULATION",
	[STATE_SERVO_CLOSE_REGULATION] = "STATE_SERVO_CLOSE_REGULATION",
	[STATE_MOTOR_REGULATION] = "STATE_MOTOR_REGULATION",
	[STATE_EMERGENCY_DISABLE] = "STATE_EMERGENCY_DISABLE",
	[STATE_ERROR] = "STATE_ERROR"
};

static void change_state(state_t state)
{
	if (state >= STATE_LAST)
	{
		return;
	}
	
	if (state != ctx.state)
	{
		LOG(PRINT_INFO, "Change state -> %s", state_name[state]);
		ctx.state = state;
	}
}

static void count_working_data(void)
{
	ctx.motor_pwm = dcmotor_process(&ctx.motorD1, ctx.motor_value);
	ctx.motor_pwm2 = dcmotor_process(&ctx.motorD2, ctx.motor_value);

	if (ctx.servo_new_value != ctx.servo_value)
	{
		ctx.servo_new_value = ctx.servo_value;
		ctx.servo_set_timer = xTaskGetTickCount() + MS2ST(750);
	}

	if (ctx.servo_set_timer < xTaskGetTickCount())
	{
		ctx.servo_set_value = ctx.servo_new_value;
	}

	ctx.servo_pwm = servo_process(ctx.servo_on ? ctx.servo_set_value : 0);
	if (ctx.motor_on) {
		motor_start(&ctx.motorD1);
		motor_start(&ctx.motorD2);
	}
	else {
		motor_stop(&ctx.motorD1);
		motor_stop(&ctx.motorD2);
	}
}

static void set_working_data(void)
{
	//#if CONFIG_DEVICE_SIEWNIK

	if (ctx.system_on)
	{
		gpio_set_level(15, 1);
	}
	else
	{
		gpio_set_level(15, 0);
	}

	LOG(PRINT_DEBUG, "motor %d %f %d", ctx.motor_on, ctx.motor_pwm, ctx.motor_value);
	if (ctx.motor_on)
	{
		float duty = (float)ctx.motor_pwm;
		LOG(PRINT_DEBUG, "duty motor %f", duty);
		mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, duty);
		mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_1); //call this each time, if operator was previously in low/high state

		mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_2, MCPWM_GEN_A, duty);
		mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_1); //call this each time, if operator was previously in low/high state
	}
	else
	{
		mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);

		mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_2, MCPWM_OPR_A);
	}

	float duty = (float)ctx.servo_pwm * 100 / 19999.0;
	LOG(PRINT_DEBUG, "duty servo %f %d %d", duty, ctx.servo_value, ctx.servo_pwm);
	mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A, duty);
	mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); //call this each time, if operator was previously in low/high state
}

static void state_init(void)
{
	mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, MOTOR_PWM_PIN);
	mcpwm_config_t pwm_config;
    pwm_config.frequency = 18000;    //frequency = 1000Hz
    pwm_config.cmpr_a = 0;       //duty cycle of PWMxA = 60.0%
    pwm_config.cmpr_b = 0;       	//duty cycle of PWMxb = 50.0%
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);   //Configure PWM0A & PWM0B with above settings

	mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM2A, MOTOR_PWM_PIN2);
    pwm_config.frequency = 18000;    //frequency = 1000Hz
    pwm_config.cmpr_a = 0;       //duty cycle of PWMxA = 60.0%
    pwm_config.cmpr_b = 0;       	//duty cycle of PWMxb = 50.0%
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_2, &pwm_config);   //Configure PWM0A & PWM0B with above settings

	mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, SERVO_PWM_PIN);
	pwm_config.frequency = 50;
    pwm_config.cmpr_a = 0;
    pwm_config.cmpr_b = 0;
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
	mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);   //Configure PWM0A & PWM0B with above settings

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1 << 15);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

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

	if (ctx.working_state_req && cmdServerIsWorking())
	{
		measure_meas_calibration_value();
		count_working_data();
		set_working_data();
		osDelay(1000);
		change_state(STATE_WORKING);
		return;
	}
	osDelay(100);
}

static void state_working(void)
{
	ctx.system_on = (uint8_t)menuGetValue(MENU_START_SYSTEM);
	ctx.servo_value = (uint8_t)menuGetValue(MENU_SERVO);
	ctx.motor_value = (uint8_t)menuGetValue(MENU_MOTOR);
	ctx.motor_on = (uint16_t)menuGetValue(MENU_MOTOR_IS_ON);
	ctx.servo_on = menuGetValue(MENU_SERVO_IS_ON) > 0;

	ctx.working_state_req = (bool)menuGetValue(MENU_START_SYSTEM);
	ctx.emergency_disable = (bool)menuGetValue(MENU_EMERGENCY_DISABLE);
	ctx.servo_open_calibration_req = (bool)menuGetValue(MENU_OPEN_SERVO_REGULATION_FLAG);
	ctx.servo_close_calibration_req = (bool)menuGetValue(MENU_CLOSE_SERVO_REGULATION_FLAG);

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

	if (!ctx.working_state_req || !cmdServerIsWorking())
	{
		change_state(STATE_IDLE);
		return;
	}

	if (ctx.servo_open_calibration_req)
	{
		change_state(STATE_SERVO_OPEN_REGULATION);
		return;
	}

	if (ctx.servo_close_calibration_req)
	{
		change_state(STATE_SERVO_CLOSE_REGULATION);
		return;
	}

	osDelay(100);
}

static void state_servo_open_regulation(void)
{
	ctx.system_on = 1;
	ctx.servo_value = 100;
	ctx.motor_value = 0;
	ctx.motor_on = 0;
	ctx.servo_on = 1;

	ctx.working_state_req = (bool)menuGetValue(MENU_START_SYSTEM);
	ctx.emergency_disable = (bool)menuGetValue(MENU_EMERGENCY_DISABLE);
	ctx.servo_open_calibration_req = (bool)menuGetValue(MENU_OPEN_SERVO_REGULATION_FLAG);

	if (ctx.emergency_disable)
	{
		menuSaveParameters();
		change_state(STATE_EMERGENCY_DISABLE);
		return;
	}

	if (!ctx.servo_open_calibration_req)
	{
		menuSaveParameters();
		change_state(STATE_IDLE);
		return;
	}

	if (!ctx.working_state_req || !cmdServerIsWorking())
	{
		menuSaveParameters();
		menuSetValue(MENU_OPEN_SERVO_REGULATION_FLAG, 0);
		change_state(STATE_IDLE);
		return;
	}

	osDelay(100);
}

static void state_servo_close_regulation(void)
{
	ctx.system_on = 1;
	ctx.servo_value = 0;
	ctx.motor_value = 0;
	ctx.motor_on = 0;
	ctx.servo_on = 1;

	ctx.working_state_req = (bool)menuGetValue(MENU_START_SYSTEM);
	ctx.emergency_disable = (bool)menuGetValue(MENU_EMERGENCY_DISABLE);
	ctx.servo_close_calibration_req = (bool)menuGetValue(MENU_CLOSE_SERVO_REGULATION_FLAG);

	if (ctx.emergency_disable)
	{
		menuSaveParameters();
		change_state(STATE_EMERGENCY_DISABLE);
		return;
	}

	if (!ctx.servo_close_calibration_req)
	{
		menuSaveParameters();
		change_state(STATE_IDLE);
		return;
	}

	if (!ctx.working_state_req || !cmdServerIsWorking())
	{
		menuSaveParameters();
		menuSetValue(MENU_OPEN_SERVO_REGULATION_FLAG, 0);
		change_state(STATE_IDLE);
		return;
	}

	osDelay(100);
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

static void state_error(void)
{
	ctx.errors = (bool)menuGetValue(MENU_MOTOR_ERROR_OVERCURRENT) || (bool)menuGetValue(MENU_SERVO_ERROR_OVERCURRENT) || 
					(bool)menuGetValue(MENU_TEMPERATURE_IS_ERROR_ON) || (bool)menuGetValue(MENU_MOTOR_ERROR_NOT_CONNECTED) || (bool)menuGetValue(MENU_SERVO_ERROR_NOT_CONNECTED);
	ctx.servo_value = 0;
	ctx.motor_value = 0;
	ctx.motor_on = false;
	ctx.servo_on = false;

	if (!ctx.errors)
	{
		errorSiewnikErrorReset();
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

			case STATE_SERVO_OPEN_REGULATION:
				state_servo_open_regulation();
				break;

			case STATE_SERVO_CLOSE_REGULATION:
				state_servo_close_regulation();
				break;

			case STATE_MOTOR_REGULATION:
				state_motor_regulation();
				break;

			case STATE_EMERGENCY_DISABLE:
				state_emergency_disable();
				break;

			case STATE_ERROR:
				state_error();
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
	motor_init(&ctx.motorD1);
	motor_init(&ctx.motorD2);
	servo_init(0);

	xTaskCreate(_task, "srvrController", 4096, NULL, 10, NULL);
}

bool srvrConrollerSetError(menuValue_t error_reason)
{
	if (ctx.state == STATE_WORKING)
	{
		change_state(STATE_ERROR);
		menuSetValue(error_reason, 1);
		return true;
	}
	return false;
}

bool srvrControllerErrorReset(void)
{
	if (ctx.state == STATE_ERROR)
	{
		errorSiewnikErrorReset();
		change_state(STATE_IDLE);
		return true;
	}
	return false;
}