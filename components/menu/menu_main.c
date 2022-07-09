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
#include "menu_low_battery.h"

#define MODULE_NAME    "[MAIN_MENU] "
#define DEBUG_LVL      PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG(_lvl, ...) \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

static menu_token_t setings =
{
    .name     = "SETINGS",
    .arg_type = T_ARG_TYPE_MENU,
    //.menu_list = setting_tokens
};

static menu_token_t start_menu =
{
    .name     = "START",
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

static menu_token_t low_battery_menu =
{
    .name     = "LOW_BAT",
    .arg_type = T_ARG_TYPE_MENU,
};

menu_token_t *main_menu_tokens[] = {&start_menu, &setings, &wifi_menu, &parameters_menu, NULL};

menu_token_t main_menu =
{
    .name      = "MENU",
    .arg_type  = T_ARG_TYPE_MENU,
    .menu_list = main_menu_tokens
};

void mainMenuInit(menu_drv_init_t init_type)
{
    if (init_type == MENU_DRV_NORMAL_INIT)
    {
        menuInitDefaultList(&main_menu);
        menuInitSettingsMenu(&setings);
        menuInitWifiMenu(&wifi_menu);
        menuInitStartMenu(&start_menu);
        menuInitParametersMenu(&parameters_menu);
        menuSetMain(&main_menu);
    }
    else
    {
        menuInitLowBatteryLvl(&low_battery_menu);
        menuSetMain(&low_battery_menu);
    }
}

void enterMenuStart(void)
{
    menuEnter(&start_menu);
}

void enterMenuParameters(void)
{
    menuEnter(&parameters_menu);
}
