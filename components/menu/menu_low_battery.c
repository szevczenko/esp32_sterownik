#include "config.h"
#include "menu.h"
#include "menu_drv.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "menu_default.h"

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
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}

	if (menu->menu_list == NULL || menu->menu_list[menu->position] == NULL) 
	{
		ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
		ssd1306_WriteString("menu->value == NULL", Font_7x10, White);
		return FALSE;
	}

	ssd1306_Fill(Black);
	ssd1306_SetCursor(2, 0);
	ssd1306_WriteString(LOGO_CLIENT_NAME, Font_11x18, White);

	ssd1306_SetCursor(2, MENU_HEIGHT);
	ssd1306_WriteString("Low battery lvl.", Font_7x10, Black);
	ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
	ssd1306_WriteString("Connect charger", Font_7x10, Black);

	return true;
}

void menuInitLowBatteryLvl(menu_token_t *menu)
{
	menu->menu_cb.enter = menu_enter_cb;
	menu->menu_cb.button_init_cb = menu_button_init_cb;
	menu->menu_cb.exit = menu_exit_cb;
	menu->menu_cb.process = menu_process;
}