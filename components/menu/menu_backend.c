#include "stdint.h"
#include "stdarg.h"

#include "config.h"
#include "menu.h"
#include "menu_drv.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "but.h"
#include "freertos/semphr.h"
#include "menu_param.h"
#include "wifidrv.h"
#include "cmd_client.h"

typedef enum
{
	STATE_INIT,
	STATE_IDLE,
	STATE_MENU_PARAMETERS,
	STATE_ERROR_CHECK,
	STATE_EMERGENCY_DISABLE,
	STATE_EMERGENCY_DISABLE_EXIT,
	STATE_TOP,
}state_backend_t;

typedef struct 
{
	state_backend_t state;
	bool error_flag;
	char * error_msg;

	bool menu_param_is_active;
	bool emergency_msg_sended;
	bool emergency_exit_msg_sended;
	bool emergensy_req;
} menu_start_context_t;

static menu_start_context_t ctx;

static char * state_name[] = 
{
	[STATE_INIT] 			= "STATE_INIT",
	[STATE_IDLE] 			= "STATE_IDLE",
	[STATE_MENU_PARAMETERS] = "STATE_MENU_PARAMETERS",
	[STATE_ERROR_CHECK] 	= "STATE_ERROR_CHECK",
	[STATE_EMERGENCY_DISABLE] = "STATE_EMERGENCY_DISABLE",
	[STATE_EMERGENCY_DISABLE_EXIT] = "STATE_EMERGENCY_DISABLE_EXIT"
};

static void change_state(state_backend_t new_state)
{
	debug_function_name(__func__);
	if (ctx.state < STATE_TOP)
	{
		if (ctx.state != new_state)
		{
			debug_msg("Backend menu %s\n\r", state_name[new_state]);
			ctx.state = new_state;
		}
	}
}

static void _enter_emergency(void)
{
	if (ctx.state != STATE_EMERGENCY_DISABLE)
	{
		debug_msg("%s %s\n\r", __func__, state_name[ctx.state]);
		change_state(STATE_EMERGENCY_DISABLE);
		ctx.emergency_msg_sended = false;
		ctx.emergency_exit_msg_sended = false;
		menuDrvEnterEmergencyDisable();
	}
}

static void _send_emergency_msg(void)
{
	if (ctx.emergency_msg_sended)
	{
		return;
	}
	int ret = cmdClientSetValue(MENU_EMERGENCY_DISABLE, 1, 2000);
	debug_msg("%s %d\n\r", __func__, ret);
	if (ret > 0)
	{
		ctx.emergency_msg_sended = true;
	}
}

static void _check_emergency_disable(void)
{
	if (ctx.emergensy_req)
	{
		_enter_emergency();
	}
}

static void backend_init_state(void)
{
	change_state(STATE_IDLE);
}

static void backend_idle(void)
{
	if (ctx.menu_param_is_active)
	{
		change_state(STATE_MENU_PARAMETERS);
		return;
	}
	osDelay(50);
}

static void backend_menu_parameters(void)
{
	if (!ctx.menu_param_is_active)
	{
		change_state(STATE_IDLE);
		return;
	}
	cmdClientGetValue(MENU_TEMPERATURE, NULL, 2000);
	cmdClientGetValue(MENU_VOLTAGE_ACCUM, NULL, 2000);
	cmdClientGetValue(MENU_CURRENT_MOTOR, NULL, 2000);
	osDelay(50);
}

static void backend_error_check(void)
{
	change_state(STATE_IDLE);
}

static void backend_emergency_disable_state(void)
{
	_send_emergency_msg();
	if (!ctx.emergensy_req)
	{
		debug_msg("%s exit\n\r", __func__);
		menuDrvExitEmergencyDisable();
		change_state(STATE_EMERGENCY_DISABLE_EXIT);
	}
	osDelay(50);
}

static void backend_emergency_disable_exit(void)
{
	if (!ctx.emergency_exit_msg_sended)
	{
		int ret = cmdClientSetValue(MENU_EMERGENCY_DISABLE, 0, 2000);
		debug_msg("%s %d\n\r", __func__, ret);
		if (ret > 0)
		{
			ctx.emergency_exit_msg_sended = true;
		}
		osDelay(50);
	}
	else
	{
		change_state(STATE_IDLE);
	}
}


void backendEnterMenuParameters(void)
{
	ctx.menu_param_is_active = true;
}

void backendExitMenuParameters(void)
{
	ctx.menu_param_is_active = false;
}

void backendToggleEmergencyDisable(void)
{
	if (ctx.emergensy_req)
	{
		ctx.emergensy_req = false;
	}
	else
	{
		if (wifiDrvIsConnected() && cmdClientIsConnected())
		{
			ctx.emergensy_req = true;
		}
	}
}

static void menu_task(void * arg)
{

	while(1)
	{		
		_check_emergency_disable();

		switch(ctx.state)
		{
			case STATE_INIT:
				backend_init_state();
				break;
			
			case STATE_IDLE:
				backend_idle();
				break;
			
			case STATE_MENU_PARAMETERS:
				backend_menu_parameters();
				break;

			case STATE_ERROR_CHECK:
				backend_error_check();
				break;

			case STATE_EMERGENCY_DISABLE:
				backend_emergency_disable_state();
				break;

			case STATE_EMERGENCY_DISABLE_EXIT:
				backend_emergency_disable_exit();
				break;

			default:
				ctx.state = STATE_IDLE;
				break;

		}
	}
}

void menuBackendInit(void)
{
	xTaskCreate(menu_task, "menu_back", 4096, NULL, 5, NULL);
}