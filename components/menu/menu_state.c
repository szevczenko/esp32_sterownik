#include "config.h"
#include "menu.h"
#include "menu_drv.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "menu_default.h"
#include "menu_param.h"
#include "wifidrv.h"
#include "cmd_client.h"
#include "menu_backend.h"

#define MODULE_NAME                       "[SETTING] "
#define DEBUG_LVL                         PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG(_lvl, ...)                          \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

typedef enum
{
	PARAM_CURRENT,
	PARAM_VOLTAGE,
	PARAM_SILOS,
	PARAM_TEMEPRATURE,
	PARAM_CONECTION,
	PARAM_SIGNAL,
	PARAM_TOP

} parameters_type_t;

typedef enum
{
	UNIT_INT,
	UNIT_DOUBLE,
	UNIT_ON_OFF,
	UNIT_BOOL,
} unit_type_t;

typedef struct 
{
	enum dictionary_phrase name_dict;
	char * unit;
	uint32_t value;
	unit_type_t unit_type;
	void (*get_value)(uint32_t *value);
} parameters_t;

static void get_current(uint32_t *value);
static void get_voltage(uint32_t *value);
static void get_silos(uint32_t *value);
static void get_signal(uint32_t *value);
static void get_temp(uint32_t *value);
static void get_conection(uint32_t *value);

static parameters_t parameters_list[] = 
{
	[PARAM_CURRENT] 	= { .name_dict = DICT_CURRENT, 	.unit = "A", 	.unit_type = UNIT_DOUBLE, 	.get_value = get_current},
	[PARAM_VOLTAGE] 	= { .name_dict = DICT_VOLTAGE, 	.unit = "V", 	.unit_type = UNIT_DOUBLE, 	.get_value = get_voltage},
	[PARAM_SILOS]		= { .name_dict = DICT_SILOS, 	.unit = "%", 	.unit_type = UNIT_INT, 		.get_value = get_silos},
	[PARAM_SIGNAL] 		= { .name_dict = DICT_SIGNAL, 	.unit = "", 	.unit_type = UNIT_INT, 		.get_value = get_signal},
	[PARAM_TEMEPRATURE] = { .name_dict = DICT_TEMP, 	.unit = "\"C", 	.unit_type = UNIT_INT, 		.get_value = get_temp},
	[PARAM_CONECTION] 	= { .name_dict = DICT_CONNECT, 	.unit = "", 	.unit_type = UNIT_BOOL, 	.get_value = get_conection}
};

static scrollBar_t scrollBar = {
	.line_max = MAX_LINE,
	.y_start = MENU_HEIGHT
};

static void get_current(uint32_t *value)
{
	*value = menuGetValue(MENU_CURRENT_MOTOR) * 100;
}

static void get_voltage(uint32_t *value)
{
	*value = menuGetValue(MENU_VOLTAGE_ACCUM);
}

static void get_silos(uint32_t *value)
{
	*value = menuGetValue(MENU_SILOS_LEVEL);
}

static void get_signal(uint32_t *value)
{
	*value = wifiDrvGetRssi();
}

static void get_temp(uint32_t *value)
{
	*value = menuGetValue(MENU_TEMPERATURE);
}

static void get_conection(uint32_t *value)
{
	*value = cmdClientIsConnected();
}

static void menu_button_up_callback(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
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
	
	menu->last_button = LAST_BUTTON_DOWN;
	if (menu->position < PARAM_TOP - 1) 
	{
		menu->position++;
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

	menu->button.enter.fall_callback = menu_button_exit_callback;
	menu->button.exit.fall_callback = menu_button_exit_callback;
	menu->button.up_minus.fall_callback = menu_button_exit_callback;
	menu->button.up_plus.fall_callback = menu_button_exit_callback;
	menu->button.down_minus.fall_callback = menu_button_exit_callback;
	menu->button.down_plus.fall_callback = menu_button_exit_callback;
	menu->button.motor_on.fall_callback = menu_button_exit_callback;

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
	backendEnterMenuParameters();
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
	backendExitMenuParameters();
	return true;
}

static bool _disconnected_process(menu_token_t *menu)
{
	menuPrintfInfo(dictionary_get_string(DICT_DEVICE_NOT_CONNECTED));
	return true;
}

static bool _connected_process(menu_token_t *menu)
{
	static char buff[64];

	for(int i = 0; i < PARAM_TOP; i++)
	{
		if (parameters_list[i].get_value != NULL)
		{
			parameters_list[i].get_value(&parameters_list[i].value);
		}
	}

	if (menu->line.end - menu->line.start != MAX_LINE - 1)
	{
		menu->line.start = menu->position;
		menu->line.end = menu->line.start + MAX_LINE - 1;
	}

	if (menu->position < menu->line.start || menu->position > menu->line.end)
	{
		if (menu->last_button == LAST_BUTTON_UP)
		{
			menu->line.start = menu->position;
			menu->line.end = menu->line.start + MAX_LINE - 1;
		}
		else
		{
			menu->line.end = menu->position;
			menu->line.start = menu->line.end - MAX_LINE + 1;
		}
		LOG(PRINT_INFO, "menu->line.start %d, menu->line.end %d, position %d, menu->last_button %d\n", menu->line.start, menu->line.end, menu->position, menu->last_button);
	}

	int line = 0;
	do
	{
		int pos = line + menu->line.start;
		
		if (parameters_list[pos].unit_type == UNIT_DOUBLE)
		{
			sprintf(buff, "%s: %.2f %s", dictionary_get_string(parameters_list[pos].name_dict), (float)parameters_list[pos].value / 100.0, parameters_list[pos].unit);
		}
		else
		{
			sprintf(buff, "%s: %d %s", dictionary_get_string(parameters_list[pos].name_dict), parameters_list[pos].value, parameters_list[pos].unit);
		}
		
		if (line + menu->line.start == menu->position)
		{
			ssdFigureFillLine(MENU_HEIGHT + LINE_HEIGHT*line, LINE_HEIGHT);
			oled_printFixedBlack(2, MENU_HEIGHT + LINE_HEIGHT*line, buff, OLED_FONT_SIZE_11);
		}
		else
		{
			oled_printFixed(2, MENU_HEIGHT + LINE_HEIGHT*line, buff, OLED_FONT_SIZE_11);
		}
		line++;
	} while (line + menu->line.start != PARAM_TOP && line < MAX_LINE);
	scrollBar.actual_line = menu->position;
	scrollBar.all_line = PARAM_TOP - 1;
	ssdFigureDrawScrollBar(&scrollBar);

	MOTOR_LED_SET_GREEN(menuGetValue(MENU_MOTOR_IS_ON));
    SERVO_VIBRO_LED_SET_GREEN(menuGetValue(MENU_SERVO_IS_ON));

	return true;
}

static bool menu_process(void * arg)
{
	menu_token_t *menu = arg;
	bool ret = false;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}

	oled_clearScreen();
	oled_setGLCDFont(OLED_FONT_SIZE_16);
    oled_printFixed(2, 0, dictionary_get_string(menu->name_dict), OLED_FONT_SIZE_16);
    oled_setGLCDFont(OLED_FONT_SIZE_11);

	if (cmdClientIsConnected())
	{
		ret = _connected_process(menu);
	}
	else
	{
		ret = _disconnected_process(menu);
	}

	return ret;
}

void menuInitParametersMenu(menu_token_t *menu)
{
	menu->menu_cb.enter = menu_enter_cb;
	menu->menu_cb.button_init_cb = menu_button_init_cb;
	menu->menu_cb.exit = menu_exit_cb;
	menu->menu_cb.process = menu_process;
}