#include "config.h"
#include "menu.h"
#include "menu_drv.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "menu_default.h"
#include "wifi_menu.h"
#include "start_menu.h"
#include "menu_state.h"
#include "menu_settings.h"

static menu_token_t setings = 
{
	.name = "SETINGS",
	.arg_type = T_ARG_TYPE_MENU,
	//.menu_list = setting_tokens
};

static menu_token_t start_menu = 
{
	.name = "START",
	.arg_type = T_ARG_TYPE_MENU,
	//.menu_list = setting_tokens
};

static menu_token_t wifi_menu = 
{
	.name = "DEVICES",
};

static menu_token_t parameters_menu = 
{
	.name = "PARAMETERS",
};

menu_token_t* main_menu_tokens[] = {&start_menu, &setings, &wifi_menu, &parameters_menu, NULL};

menu_token_t main_menu = 
{
	.name = "MENU",
	.arg_type = T_ARG_TYPE_MENU,
	.menu_list = main_menu_tokens
};

void mainMenuInit(void)
{
	menuInitDefaultList(&main_menu);
	menuInitSettingsMenu(&setings);
	menuInitWifiMenu(&wifi_menu);
	menuInitStartMenu(&start_menu);
	menuInitParametersMenu(&parameters_menu);
	menuSetMain(&main_menu);
}