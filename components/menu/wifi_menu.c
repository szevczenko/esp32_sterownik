#include "config.h"
#include "menu.h"
#include "menu_drv.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "menu_default.h"

#include "wifidrv.h"
#include "cmd_client.h"

#define DEVICE_LIST_SIZE 32

typedef enum
{
	ST_WIFI_INIT,
	ST_WIFI_IDLE,
	ST_WIFI_FIND_DEVICE,
	ST_WIFI_DEVICE_LIST,
	ST_WIFI_DEVICE_TRY_CONNECT,
	ST_WIFI_DEVICE_WAIT_CONNECT,
	ST_WIFI_DEVICE_WAIT_CMD_CLIENT,
	ST_WIFI_CONNECTED,
	ST_WIFI_ERROR_CHECK,
	ST_WIFI_STOP

}stateWifiMenu_t;

typedef struct 
{
	stateWifiMenu_t state;
	uint16_t ap_count;
	uint16_t devices_count;
	uint32_t timeout_con;
	char devices_list[DEVICE_LIST_SIZE][33];
	bool connect_req;
	bool exit_req;
	bool scan_req;
	bool error_flag;
	int error_code;
	char * error_msg;
}wifi_menu_t;

static wifi_menu_t ctx;

static char *state_name[] = 
{
	[ST_WIFI_INIT] = "ST_WIFI_INIT",
	[ST_WIFI_IDLE] = "ST_WIFI_IDLE",
	[ST_WIFI_FIND_DEVICE] = "ST_WIFI_FIND_DEVICE",
	[ST_WIFI_DEVICE_LIST] = "ST_WIFI_DEVICE_LIST",
	[ST_WIFI_DEVICE_TRY_CONNECT] = "ST_WIFI_DEVICE_TRY_CONNECT",
	[ST_WIFI_DEVICE_WAIT_CONNECT] = "ST_WIFI_DEVICE_WAIT_CONNECT",
	[ST_WIFI_DEVICE_WAIT_CMD_CLIENT] = "ST_WIFI_DEVICE_WAIT_CMD_CLIENT",
	[ST_WIFI_CONNECTED] = "ST_WIFI_CONNECTED",
	[ST_WIFI_ERROR_CHECK] = "ST_WIFI_ERROR_CHECK",
	[ST_WIFI_STOP] = "ST_WIFI_STOP"
};

static scrollBar_t scrollBar = {
	.line_max = MAX_LINE,
	.y_start = MENU_HEIGHT
};

static void change_state(stateWifiMenu_t new_state)
{
	ctx.state = new_state;
	debug_msg("WiFi menu %s\n\r", state_name[new_state]);
}

static void menu_button_up_callback(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	if (ctx.state != ST_WIFI_DEVICE_LIST)
	{
		return;
	}

	menu->last_button = LAST_BUTTON_UP;
	if (menu->position > 0) 
	{
		menu->position--;
	}
}

static void menu_button_down_callback(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	if (ctx.state != ST_WIFI_DEVICE_LIST)
	{
		return;
	}
	
	menu->last_button = LAST_BUTTON_DOWN;
	if (menu->position < ctx.devices_count - 1) 
	{
		menu->position++;
	}
}

static void menu_button_enter_callback(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	if (ctx.state == ST_WIFI_IDLE || ctx.state == ST_WIFI_ERROR_CHECK)
	{
		ctx.scan_req = true;
	}
	else if (ctx.state == ST_WIFI_DEVICE_LIST)
	{
		if (ctx.devices_count == 0)
		{
			ctx.scan_req = true;
		}
		else
		{
			ctx.connect_req = true;
		}
	}
}

static void menu_button_exit_callback(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}
	menuExit(menu);
}

static bool menu_button_init_cb(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}
	menu->button.down.fall_callback = menu_button_down_callback;
	menu->button.up.fall_callback = menu_button_up_callback;
	menu->button.enter.fall_callback = menu_button_enter_callback;
	menu->button.exit.fall_callback = menu_button_exit_callback;
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
	ctx.scan_req = true;
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

static bool connectToDevice(char *dev)
{
	printf("Try connect %s \n\r", dev);
	if (memcmp(WIFI_AP_NAME, dev, strlen(WIFI_AP_NAME) - 1) == 0)
	{
		/* Disconnect if connected */
		if (wifiDrvIsConnected())
		{
			if (wifiDrvDisconnect() != ESP_OK){
				ctx.error_msg = "conToDevice err";
				return false;
			}
		}
		wifiDrvSetAPName(dev, strlen(dev) + 1);
		wifiDrvSetPassword(WIFI_AP_PASSWORD, strlen(WIFI_AP_PASSWORD));

		/* Wait to wifi drv ready connect */
		uint32_t wait_to_ready = MS2ST(1000) + xTaskGetTickCount();
		do
		{
			if (wait_to_ready < xTaskGetTickCount())
			{
				ctx.error_msg = "Wifi drv not ready";
				return false;
			}
			osDelay(50);
		} while (!wifiDrvReadyToConnect());
		
		if (wifiDrvConnect() != TRUE) 
		{
			ctx.error_msg = "conToDevice err";
			return false;
		}
		return true;
	}
	ctx.error_msg = "Bad dev name";
	return false;
}

static void menu_wifi_init(void)
{
	if (wifiDrvIsReadyToScan())
	{
		change_state(ST_WIFI_IDLE);
	}
	ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
	ssd1306_WriteString("Wait to WiFi init", Font_7x10, White);
}

static void menu_wifi_idle(void)
{
	if (ctx.scan_req)
	{
		ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
		ssd1306_WriteString("Scanning devices...", Font_7x10, White);
		change_state(ST_WIFI_FIND_DEVICE);
		return;
	}

	ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
	ssd1306_WriteString("Click enter to", Font_7x10, White);
	ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
	ssd1306_WriteString("scanning devices", Font_7x10, White);
}

static void menu_wifi_find_devices(void)
{
	static char dev_name[33] = {0};

	int err = wifiDrvStartScan();
	if (err == ESP_OK)
	{
		ctx.devices_count = 0;
		wifiDrvGetScanResult(&ctx.ap_count);
		for (uint16_t i = 0; i < ctx.ap_count; i++)
		{
			if (ctx.ap_count > DEVICE_LIST_SIZE)
			{
				break;
			}
			wifiDrvGetNameFromScannedList(i, dev_name);
			if (memcmp(dev_name, WIFI_AP_NAME, strlen(WIFI_AP_NAME) - 1) == 0)
			{
				debug_msg("%s\n", dev_name);
				strcpy(ctx.devices_list[ctx.devices_count++], dev_name);
			}
		}
		change_state(ST_WIFI_DEVICE_LIST);
	}
	else
	{
		change_state(ST_WIFI_ERROR_CHECK);
		ctx.error_flag = true;
		ctx.error_code = err;
		ctx.error_msg = "WiFi Scan Error";
	}
	ctx.scan_req = false;
}

static void menu_wifi_show_list(menu_token_t *menu)
{
	if (ctx.devices_count == 0)
	{
		if (ctx.scan_req)
		{
			ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
			ssd1306_WriteString("Find devices", Font_7x10, White);
			change_state(ST_WIFI_FIND_DEVICE);
			return;
		}
		ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
		ssd1306_WriteString("Devices not found.", Font_7x10, White);
		ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
		ssd1306_WriteString("Click button for", Font_7x10, White);
		ssd1306_SetCursor(2, MENU_HEIGHT + 3*LINE_HEIGHT);
		ssd1306_WriteString("try find device", Font_7x10, White);
		return;
	}

	if (ctx.connect_req)
	{
		ctx.connect_req = false;
		ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
		ssd1306_WriteString("Try connect to:", Font_7x10, White);
		ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
		ssd1306_WriteString(ctx.devices_list[menu->position], Font_7x10, White);
		change_state(ST_WIFI_DEVICE_TRY_CONNECT);
		return;
	}

	if (menu->line.end - menu->line.start != MAX_LINE - 1)
	{
		menu->line.start = menu->position;
		menu->line.end = menu->line.start + MAX_LINE - 1;
	}

	if (menu->position < menu->line.start || menu->position > menu->line.end)
	{
		if (menu->last_button)
		{
			menu->line.start = menu->position;
			menu->line.end = menu->line.start + MAX_LINE - 1;
		}
		else
		{
			menu->line.end = menu->position;
			menu->line.start = menu->line.end - MAX_LINE + 1;
		}
	}
	//debug_msg("position %d, ctx.devices_count %d menu->line.start %d\n", menu->position, ctx.devices_count, menu->line.start);
	int line = 0;
	do
	{
		ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT*line);
		if (line + menu->line.start == menu->position)
		{
			ssdFigureFillLine(MENU_HEIGHT + LINE_HEIGHT*line, LINE_HEIGHT);
			ssd1306_WriteString(&ctx.devices_list[line + menu->line.start][6], Font_7x10, Black);
		}
		else
		{
			ssd1306_WriteString(&ctx.devices_list[line + menu->line.start][6], Font_7x10, White);
		}
		
		line++;
	} while (line + menu->line.start < ctx.devices_count && line < MAX_LINE);
	scrollBar.actual_line = menu->position;
	scrollBar.all_line = ctx.devices_count - 1;
	ssdFigureDrawScrollBar(&scrollBar);
}

static void menu_wifi_connect(menu_token_t *menu)
{
	if (connectToDevice(ctx.devices_list[menu->position]))
	{
		ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
		ssd1306_WriteString("Wait to connect", Font_7x10, White);
		ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
		ssd1306_WriteString(ctx.devices_list[menu->position], Font_7x10, White);
		change_state(ST_WIFI_DEVICE_WAIT_CONNECT);
	}
	else
	{
		ctx.error_flag = 1;
		ctx.error_msg = "Error connected";
		change_state(ST_WIFI_ERROR_CHECK);
	}
}

static void menu_wifi_wait_connect(void)
{
	/* Wait to connect wifi */
	ctx.timeout_con = MS2ST(10000) + xTaskGetTickCount();
	do
	{
		if (ctx.timeout_con < xTaskGetTickCount())
		{
			ctx.error_msg = "Timeout connect";
			ctx.error_flag = 1;
			change_state(ST_WIFI_ERROR_CHECK);
			return;
		}
		osDelay(50);
	} while (wifiDrvTryingConnect());

	if (wifiDrvIsConnected())
	{
		ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
		ssd1306_WriteString("WiFi connected.", Font_7x10, White);
		ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
		ssd1306_WriteString("Wait to server...", Font_7x10, White);
		change_state(ST_WIFI_DEVICE_WAIT_CMD_CLIENT);
	}
	else
	{
		ctx.error_msg = "Error connect";
		ctx.error_flag = 1;
		change_state(ST_WIFI_ERROR_CHECK);
	}
}

static void menu_wifi_wait_cmd_client(void)
{
	/* Wait to cmd server */
	ctx.timeout_con = MS2ST(10000) + xTaskGetTickCount();
	do
	{
		if (ctx.timeout_con < xTaskGetTickCount())
		{
			ctx.error_msg = "Timeout server";
			ctx.error_flag = 1;
			change_state(ST_WIFI_ERROR_CHECK);
			return;
		}
		osDelay(50);
	} while (!cmdClientIsConnected());

	change_state(ST_WIFI_CONNECTED);
}

static void menu_wifi_error_check(void)
{
	static char error_buff[64];
	if (ctx.error_flag)
	{
		ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
		ssd1306_WriteString("Error", Font_7x10, White);
		if (ctx.error_msg != NULL)
		{
			sprintf(error_buff, "%s:%d", ctx.error_msg, ctx.error_code);
			ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
			debug_msg("Wifi error [%d] %s\n\r", ctx.error_code, ctx.error_msg);
		}
		wifiDrvDisconnect();
		if (ctx.scan_req)
		{
			change_state(ST_WIFI_STOP);
		}
		else
		{
			return;
		}
	}
	else
	{
		change_state(ST_WIFI_IDLE);
	}
	ctx.error_flag = false;
	ctx.error_msg = NULL;
	ctx.error_code = 0;
}

static void menu_wifi_connected(menu_token_t *menu)
{
	ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
	ssd1306_WriteString("Connected to:", Font_7x10, White);
	ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
	ssd1306_WriteString(ctx.devices_list[menu->position], Font_7x10, White);
	ssd1306_UpdateScreen();

	if (ctx.scan_req)
	{
		change_state(ST_WIFI_STOP);
	}
	else
	{
		osDelay(1000);
		menuExit(menu);
		enterMenuStart();
	}
}

static void menu_wifi_stop(void)
{
	wifiDrvDisconnect();
	change_state(ST_WIFI_INIT);
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
		case ST_WIFI_INIT:
			menu_wifi_init();
			break;
		
		case ST_WIFI_IDLE:
			menu_wifi_idle();
			break;

		case ST_WIFI_FIND_DEVICE:
			menu_wifi_find_devices();
			break;

		case ST_WIFI_DEVICE_LIST:
			menu_wifi_show_list(menu);
			break;

		case ST_WIFI_DEVICE_TRY_CONNECT:
			menu_wifi_connect(menu);
			break;

		case ST_WIFI_DEVICE_WAIT_CONNECT:
			menu_wifi_wait_connect();
			break;

		case ST_WIFI_DEVICE_WAIT_CMD_CLIENT:
			menu_wifi_wait_cmd_client();
			break;

		case ST_WIFI_CONNECTED:
			menu_wifi_connected(menu);
			break;

		case ST_WIFI_STOP:
			menu_wifi_stop();
			break;

		case ST_WIFI_ERROR_CHECK:
			menu_wifi_error_check();
			break;

		default:
			change_state(ST_WIFI_STOP);
			break;	
	}

	return true;
}

void menuInitWifiMenu(menu_token_t *menu)
{
	memset(&ctx, 0, sizeof(ctx));
	menu->menu_cb.enter = menu_enter_cb;
	menu->menu_cb.button_init_cb = menu_button_init_cb;
	menu->menu_cb.exit = menu_exit_cb;
	menu->menu_cb.process = menu_process;
}