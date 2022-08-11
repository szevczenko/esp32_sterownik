#include "config.h"
#include "menu.h"
#include "menu_drv.h"
// #include "ssd1306.h"
#include "ssdFigure.h"
#include "menu_default.h"
#include "oled.h"

#define MODULE_NAME    "[DEFAULT] "
#define DEBUG_LVL      PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG(_lvl, ...) \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

static scrollBar_t scrollBar =
{
    .line_max = MAX_LINE,
    .y_start  = MENU_HEIGHT
};

static void menu_button_up_callback(void *arg)
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

static void menu_button_down_callback(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    menu->last_button = LAST_BUTTON_DOWN;
    if (menu->position < menuDrvElementsCnt(menu) - 1)
    {
        menu->position++;
    }
}

static void menu_button_enter_callback(void *arg)
{
    menu_token_t *menu = arg;

    if ((menu == NULL) || (menu->menu_list == NULL) || (menu->menu_list[menu->position] == NULL))
    {
        NULL_ERROR_MSG();
        return;
    }

    menuEnter(menu->menu_list[menu->position]);
}

static void menu_button_exit_callback(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    // menuExit(menu);
    menuEnter(menu->menu_list[0]);
}

static bool menu_button_init_cb(void *arg)
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

    if ((menu->menu_list == NULL) || (menu->menu_list[menu->position] == NULL))
    {
        oled_setCursor(2, MENU_HEIGHT + 2 * LINE_HEIGHT);
        oled_printFixed(2, MENU_HEIGHT + 2 * LINE_HEIGHT, "menu->value == NULL", OLED_FONT_SIZE_11);
        return FALSE;
    }

    oled_clearScreen();
    oled_printFixed(2, 0, menu->name, OLED_FONT_SIZE_16);
    oled_setGLCDFont(OLED_FONT_SIZE_11);

    if (menu->line.end - menu->line.start != MAX_LINE - 1)
    {
        menu->line.start = menu->position;
        menu->line.end = menu->line.start + MAX_LINE - 1;
    }

    if ((menu->position < menu->line.start) || (menu->position > menu->line.end))
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
    }

    int line = 0;

    do
    {
        if (line + menu->line.start == menu->position)
        {
            ssdFigureFillLine(MENU_HEIGHT + LINE_HEIGHT * line, LINE_HEIGHT);
            oled_printFixedBlack(2, MENU_HEIGHT + LINE_HEIGHT * line, menu->menu_list[line + menu->line.start]->name, STYLE_NORMAL);
        }
        else
        {
            oled_printFixed(2, MENU_HEIGHT + LINE_HEIGHT * line, menu->menu_list[line + menu->line.start]->name, STYLE_NORMAL);
        }

        line++;
    } while (menu->menu_list[line + menu->line.start] != NULL && line < MAX_LINE);

    scrollBar.actual_line = menu->position;
    scrollBar.all_line = menuDrvElementsCnt(menu) - 1;
    ssdFigureDrawScrollBar(&scrollBar);

    return true;
}

void menuInitDefaultList(menu_token_t *menu)
{
    menu->menu_cb.enter = menu_enter_cb;
    menu->menu_cb.button_init_cb = menu_button_init_cb;
    menu->menu_cb.exit = menu_exit_cb;
    menu->menu_cb.process = menu_process;
}
