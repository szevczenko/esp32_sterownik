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
#include "menu_settings.h"

typedef enum
{
	MENU_LIST_PARAMETERS,
	MENU_EDIT_PARAMETERS,
	MENU_TOP
} menu_state_t;

typedef enum
{
	PARAM_BOOTUP_MENU,
	PARAM_BUZZER,
	PARAM_SERVO_CALIBRATION,
	PARAM_TOP
} parameters_type_t;

typedef enum
{
	UNIT_INT,
	UNIT_ON_OFF,
	UNIT_BOOL,
} unit_type_t;

typedef struct 
{
	char * name;
	uint32_t value;
	uint32_t max_value;
	unit_type_t unit_type;
	void (*get_value)(uint32_t *value);
	void (*get_max_value)(uint32_t *value);
	void (*set_value)(uint32_t value);
} parameters_t;

static void get_bootup(uint32_t *value);
static void get_buzzer(uint32_t *value);
static void get_servo_calibration(uint32_t *value);
static void get_max_bootup(uint32_t *value);
static void get_max_buzzer(uint32_t *value);
static void get_max_servo_calibration(uint32_t *value);
static void set_bootup(uint32_t value);
static void set_buzzer(uint32_t value);
static void set_servo_calibration(uint32_t value);

static parameters_t parameters_list[] = 
{
	[PARAM_BOOTUP_MENU] 		= { .name = "Booting", .unit_type = UNIT_ON_OFF, .get_value = get_bootup, .set_value = set_bootup, .get_max_value = get_max_bootup},
	[PARAM_BUZZER] 				= { .name = "Buzzer", .unit_type = UNIT_ON_OFF, .get_value = get_buzzer, .set_value = set_buzzer, .get_max_value = get_max_buzzer},
	[PARAM_SERVO_CALIBRATION] 	= { .name = "Servo calibration", .unit_type = UNIT_INT, .get_value = get_servo_calibration, .set_value = set_servo_calibration, .get_max_value = get_max_servo_calibration},
};

static scrollBar_t scrollBar = {
	.line_max = MAX_LINE,
	.y_start = MENU_HEIGHT
};

static menu_state_t _state;

static void get_bootup(uint32_t *value)
{
	*value = menuGetValue(MENU_BOOTUP_SYSTEM);
}

static void get_buzzer(uint32_t *value)
{
	*value = menuGetValue(MENU_BUZZER);
}

static void get_servo_calibration(uint32_t *value)
{
	*value = menuGetValue(MENU_CLOSE_SERVO_REGULATION);
}

static void get_max_bootup(uint32_t *value)
{
	*value = menuGetMaxValue(MENU_BOOTUP_SYSTEM);
}

static void get_max_buzzer(uint32_t *value)
{
	*value = menuGetMaxValue(MENU_BUZZER);
}

static void get_max_servo_calibration(uint32_t *value)
{
	*value = menuGetMaxValue(MENU_CLOSE_SERVO_REGULATION);
}

static void set_bootup(uint32_t value)
{
	menuSetValue(MENU_BOOTUP_SYSTEM, value);
}

static void set_buzzer(uint32_t value)
{
	menuSetValue(MENU_BUZZER, value);
}

static void set_servo_calibration(uint32_t value)
{
	menuSetValue(MENU_CLOSE_SERVO_REGULATION, value);
}

static void menu_button_up_callback(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	if (_state == MENU_EDIT_PARAMETERS)
	{
		if (parameters_list[menu->position].set_value != NULL)
		{
			parameters_list[menu->position].set_value(parameters_list[menu->position].value);
		}
	}

	menu->last_button = LAST_BUTTON_UP;
	if (menu->position > 0) 
	{
		menu->position--;
	}

	if (_state == MENU_EDIT_PARAMETERS)
	{
		if (parameters_list[menu->position].get_value != NULL)
		{
			parameters_list[menu->position].get_value(&parameters_list[menu->position].value);
		}

		if (parameters_list[menu->position].get_max_value != NULL)
		{
			parameters_list[menu->position].get_max_value(&parameters_list[menu->position].max_value);
		}
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

	if (_state == MENU_EDIT_PARAMETERS)
	{
		if (parameters_list[menu->position].set_value != NULL)
		{
			parameters_list[menu->position].set_value(parameters_list[menu->position].value);
		}
	}
	
	menu->last_button = LAST_BUTTON_DOWN;
	if (menu->position < PARAM_TOP - 1) 
	{
		menu->position++;
	}

	if (_state == MENU_EDIT_PARAMETERS)
	{
		if (parameters_list[menu->position].get_value != NULL)
		{
			parameters_list[menu->position].get_value(&parameters_list[menu->position].value);
		}

		if (parameters_list[menu->position].get_max_value != NULL)
		{
			parameters_list[menu->position].get_max_value(&parameters_list[menu->position].max_value);
		}
	}
}

static void menu_button_plus_callback(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	if (menu->position >= PARAM_TOP) 
	{
		return;
	}

	if (parameters_list[menu->position].value < parameters_list[menu->position].max_value)
	{
		parameters_list[menu->position].value++;
	}
}

static void menu_button_minus_callback(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	if (menu->position >= PARAM_TOP) 
	{
		return;
	}

	if (parameters_list[menu->position].value > 0)
	{
		parameters_list[menu->position].value--;
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

	if (menu->position >= PARAM_TOP)
	{
		debug_msg("Error settings: menu->position >= PARAM_TOP");
		return;
	}

	if (_state == MENU_EDIT_PARAMETERS)
	{
		_state = MENU_LIST_PARAMETERS;
		if (parameters_list[menu->position].set_value != NULL)
		{
			parameters_list[menu->position].set_value(parameters_list[menu->position].value);
		}
	}
	else
	{
		_state = MENU_EDIT_PARAMETERS;
		if (parameters_list[menu->position].get_value != NULL)
		{
			parameters_list[menu->position].get_value(&parameters_list[menu->position].value);
		}

		if (parameters_list[menu->position].get_max_value != NULL)
		{
			parameters_list[menu->position].get_max_value(&parameters_list[menu->position].max_value);
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
	menuSaveParameters();
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
	menu->button.up_minus.fall_callback = menu_button_minus_callback;
	menu->button.up_plus.fall_callback = menu_button_plus_callback;
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

static bool menu_process(void * arg)
{
	static char buff[64];
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}

	ssd1306_Fill(Black);

	switch (_state)
	{
	case MENU_LIST_PARAMETERS:
	{
		ssd1306_SetCursor(2, 0);
		ssd1306_WriteString(menu->name, Font_11x18, White);

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
			debug_msg("menu->line.start %d, menu->line.end %d, position %d, menu->last_button %d\n", menu->line.start, menu->line.end, menu->position, menu->last_button);
		}

		int line = 0;
		do
		{
			ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT*line);
			int pos = line + menu->line.start;
			sprintf(buff, "%s", parameters_list[pos].name);
			if (line + menu->line.start == menu->position)
			{
				ssdFigureFillLine(MENU_HEIGHT + LINE_HEIGHT*line, LINE_HEIGHT);
				ssd1306_WriteString(buff, Font_7x10, Black);
			}
			else
			{
				ssd1306_WriteString(buff, Font_7x10, White);
			}
			line++;
		} while (line + menu->line.start != PARAM_TOP && line < MAX_LINE);
		scrollBar.actual_line = menu->position;
		scrollBar.all_line = PARAM_TOP - 1;
		ssdFigureDrawScrollBar(&scrollBar);
	}
	break;

	case MENU_EDIT_PARAMETERS:
		ssd1306_SetCursor(2, 0);
		ssd1306_WriteString(parameters_list[menu->position].name, Font_11x18, White);
		switch (parameters_list[menu->position].unit_type)
		{
			case UNIT_INT:
				ssd1306_SetCursor(30, MENU_HEIGHT + LINE_HEIGHT * 2);
				sprintf(buff, "%d", parameters_list[menu->position].value);
				ssd1306_WriteString(buff, Font_7x10, White);
			break;

			case UNIT_ON_OFF:
				ssd1306_SetCursor(30, MENU_HEIGHT + LINE_HEIGHT * 2);
				sprintf(buff, "%s", parameters_list[menu->position].value ? "ON" : "OFF");
				ssd1306_WriteString(buff, Font_7x10, White);
			break;

			case UNIT_BOOL:
				sprintf(buff, "%s", parameters_list[menu->position].value ? "1" : "0");
				ssd1306_WriteString(buff, Font_7x10, White);
				ssd1306_SetCursor(30, MENU_HEIGHT + LINE_HEIGHT * 2);
			break;
		
		default:
			break;
		}
		break;
	
	default:
		_state = MENU_LIST_PARAMETERS;
		break;
	}

	return true;
}

void menuInitSettingsMenu(menu_token_t *menu)
{
	menu->menu_cb.enter = menu_enter_cb;
	menu->menu_cb.button_init_cb = menu_button_init_cb;
	menu->menu_cb.exit = menu_exit_cb;
	menu->menu_cb.process = menu_process;
}