#include "config.h"
#include "menu.h"
#include "menu_drv.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "menu_default.h"
#include "power_on.h"

#define MODULE_NAME    "[M_LOW_BAT] "
#define DEBUG_LVL      PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG(_lvl, ...) \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

static bool menu_button_init_cb(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return false;
    }

    return true;
}

static bool menu_enter_cb(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return false;
    }

    return true;
}

static bool menu_exit_cb(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return false;
    }

    return true;
}

static bool menu_process(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return false;
    }

    oled_clearScreen();
    oled_printFixed(2, MENU_HEIGHT, "LOW BATTERY", STYLE_NORMAL); //11x18 White
    oled_printFixed(2, 2 * MENU_HEIGHT + 5, " Connect charger", STYLE_NORMAL);
    oled_update();

    return true;
}

void menuInitLowBatteryLvl(menu_token_t *menu)
{
    menu->menu_cb.enter = menu_enter_cb;
    menu->menu_cb.button_init_cb = menu_button_init_cb;
    menu->menu_cb.exit = menu_exit_cb;
    menu->menu_cb.process = menu_process;
}
