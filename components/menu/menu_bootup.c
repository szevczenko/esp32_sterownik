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
#include "menu_default.h"
#include "cmd_client.h"
#include "parse_cmd.h"

typedef enum
{
	STATE_INIT,
	STATE_WAIT_WIFI_INIT,
	STATE_CHECK_MEMORY,
	STATE_CONNECT,
	STATE_WAIT_CONNECT,
	STATE_GET_SERVER_DATA,
	STATE_CHECKING_DATA,
	STATE_EXIT,
	STATE_TOP,
}state_bootup_t;

typedef struct 
{
	state_bootup_t state;
	bool error_flag;
	char * error_msg;
	char ap_name[33];
	uint32_t timeout_con;
	bool system_connected;
} menu_start_context_t;

static menu_start_context_t ctx;

static char * state_name[] = 
{
	[STATE_INIT] 			= "STATE_INIT",
	[STATE_WAIT_WIFI_INIT] 	= "STATE_WAIT_WIFI_INIT",
	[STATE_CHECK_MEMORY] 	= "STATE_CHECK_MEMORY",
	[STATE_CONNECT] 		= "STATE_CONNECT",
	[STATE_WAIT_CONNECT] 	= "STATE_WAIT_CONNECT",
	[STATE_GET_SERVER_DATA] = "STATE_GET_SERVER_DATA",
	[STATE_CHECKING_DATA]	= "STATE_CHECKING_DATA",
	[STATE_EXIT] 			= "STATE_EXIT",
};

extern void mainMenuInit(void);

menu_token_t bootup_menu = 
{
	.name = "STARTING...",
	.arg_type = T_ARG_TYPE_MENU,
};

static void change_state(state_bootup_t new_state)
{
	debug_function_name(__func__);
	if (ctx.state < STATE_TOP)
	{
		if (ctx.state != new_state)
		{
			debug_msg("Bootup menu %s\n\r", state_name[new_state]);
		}
		ctx.state = new_state;
	}
	else
	{
		debug_msg("ERROR: change state %d\n\r", new_state);
	}
}

static void bootup_init_state(void)
{
	menuPrintfInfo("Init");
	change_state(STATE_WAIT_WIFI_INIT);
}

static void bootup_wifi_wait(void)
{
	if(wifiDrvReadyToConnect())
	{
		change_state(STATE_CHECK_MEMORY);
	}
	else
	{
		menuPrintfInfo("Wait to start:\nWiFi");
	}
}

static void bootup_check_memory(void)
{
	if (wifiDrvIsReadedData())
	{
		change_state(STATE_CONNECT);
	}
	else
	{
		change_state(STATE_EXIT);
	}
}

static void bootup_connect(void)
{
	wifiDrvGetAPName(ctx.ap_name);
	menuPrintfInfo("Try connect to:\n%s", ctx.ap_name);
	wifiDrvConnect();
	change_state(STATE_WAIT_CONNECT);
}

static void bootup_wait_connect(void)
{
	/* Wait to connect wifi */
	ctx.timeout_con = MS2ST(10000) + xTaskGetTickCount();
	do
	{
		if (ctx.timeout_con < xTaskGetTickCount())
		{
			ctx.error_msg = "Timeout connect";
			ctx.error_flag = 1;
			change_state(STATE_EXIT);
			return;
		}
		osDelay(50);
	} while (wifiDrvTryingConnect());

	ctx.timeout_con = MS2ST(10000) + xTaskGetTickCount();
	do
	{
		if (ctx.timeout_con < xTaskGetTickCount())
		{
			ctx.error_msg = "Timeout server";
			ctx.error_flag = 1;
			change_state(STATE_EXIT);
			return;
		}
		osDelay(50);
	} while (!cmdClientIsConnected());

	menuPrintfInfo("Connected:\n%s\n Try read data", ctx.ap_name);
	change_state(STATE_GET_SERVER_DATA);
}

static void bootup_get_server_data(void)
{
	uint32_t time_to_connect = 0;
	uint32_t start_status;

	while(cmdClientGetValue(MENU_START_SYSTEM, &start_status, 2000) == 0) 
	{
		if (time_to_connect < 5) 
		{
			time_to_connect++;
		}
		else 
		{
			debug_msg("Timeout get MENU_START_SYSTEM\n\r");
			change_state(STATE_EXIT);
			return;
		}
	}
	
	if (start_status == 0) 
	{
		debug_msg("Bootup: System not started\n\r");
		change_state(STATE_EXIT);
		return;
	}

	if (cmdClientGetAllValue(2500) == 0) {
		debug_msg("Timeout get ALL VALUES\n\r");
		change_state(STATE_EXIT);
	}

	menuPrintfInfo("Read data from:\n%s", ctx.ap_name);
	change_state(STATE_CHECKING_DATA);
}

static void bootup_checking_data(void)
{
	ctx.system_connected = true;
	change_state(STATE_EXIT);
	menuPrintfInfo("System ready to start");
}

static void bootup_exit(void)
{
	mainMenuInit();
}

static bool menu_process(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}

	ssd1306_Fill(Black);
	ssd1306_SetCursor(2, 0);
	ssd1306_WriteString(menu->name, Font_11x18, White);
	
	switch(ctx.state)
	{
		case STATE_INIT:
			bootup_init_state();
			break;
		
		case STATE_WAIT_WIFI_INIT:
			bootup_wifi_wait();
			break;

		case STATE_CHECK_MEMORY:
			bootup_check_memory();
			break;
			
		case STATE_CONNECT:
			bootup_connect();
			break;
			
		case STATE_WAIT_CONNECT:
			bootup_wait_connect();
			break;

		case STATE_GET_SERVER_DATA:
			bootup_get_server_data();
			break;

		case STATE_CHECKING_DATA:
			bootup_checking_data();
			break;

		case STATE_EXIT:
			bootup_exit();
			break;

		default:
			ctx.state = STATE_CONNECT;
			break;

	}

	return true;
}

static bool menu_enter_cb(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}
	return true;
}

static bool menu_exit_cb(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}
	return true;
}

static bool menu_button_init_cb(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}
	return true;
}

void menuInitBootupMenu(void)
{
	memset(&ctx, 0, sizeof(ctx));
	bootup_menu.menu_cb.enter = menu_enter_cb;
	bootup_menu.menu_cb.button_init_cb = menu_button_init_cb;
	bootup_menu.menu_cb.exit = menu_exit_cb;
	bootup_menu.menu_cb.process = menu_process;

	menuEnter(&bootup_menu);
}