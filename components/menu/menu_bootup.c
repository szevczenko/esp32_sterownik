#include "stdint.h"
#include "stdarg.h"

#include "config.h"
#include "menu.h"
#include "menu_drv.h"
// #include "ssd1306.h"
#include "ssdFigure.h"
#include "but.h"
#include "freertos/semphr.h"
#include "menu_param.h"
#include "wifidrv.h"
#include "menu_default.h"
#include "cmd_client.h"
#include "parse_cmd.h"
#include "oled.h"

#define MODULE_NAME    "[BOOTUP] "
#define DEBUG_LVL      PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG(_lvl, ...) \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

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
} state_bootup_t;

typedef struct
{
    state_bootup_t state;
    bool error_flag;
    char *error_msg;
    char ap_name[33];
    uint32_t timeout_con;
    bool system_connected;
    char buff[128];
} menu_start_context_t;

static menu_start_context_t ctx;

static char *state_name[] =
{
    [STATE_INIT] = "STATE_INIT",
    [STATE_WAIT_WIFI_INIT] = "STATE_WAIT_WIFI_INIT",
    [STATE_CHECK_MEMORY] = "STATE_CHECK_MEMORY",
    [STATE_CONNECT] = "STATE_CONNECT",
    [STATE_WAIT_CONNECT] = "STATE_WAIT_CONNECT",
    [STATE_GET_SERVER_DATA] = "STATE_GET_SERVER_DATA",
    [STATE_CHECKING_DATA] = "STATE_CHECKING_DATA",
    [STATE_EXIT] = "STATE_EXIT",
};

extern void mainMenuInit(menu_drv_init_t init_type);
extern void enterMenuStart(void);

menu_token_t bootup_menu =
{
    .name     = LOGO_CLIENT_NAME,
    .arg_type = T_ARG_TYPE_MENU,
};

static void change_state(state_bootup_t new_state)
{
    debug_function_name(__func__);
    if (ctx.state < STATE_TOP)
    {
        if (ctx.state != new_state)
        {
            LOG(PRINT_INFO, "Bootup menu %s", state_name[new_state]);
        }

        ctx.state = new_state;
    }
    else
    {
        LOG(PRINT_ERROR, "change state %d", new_state);
    }
}

static void bootup_init_state(void)
{
    menuPrintfInfo("Init");
    change_state(STATE_WAIT_WIFI_INIT);
}

static void bootup_wifi_wait(void)
{
    if (wifiDrvReadyToConnect())
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

static void _show_wait_connection(void)
{
    sprintf(ctx.buff, "Wait connection%s%s%s", xTaskGetTickCount() % 400 > 100 ? "." : " ",
        xTaskGetTickCount() % 400 > 200 ? "." : " ", xTaskGetTickCount() % 400 > 300 ? "." : " ");
    oled_setCursor(2, MENU_HEIGHT + 2 * LINE_HEIGHT);
    oled_printFixed(2, MENU_HEIGHT + 2 * LINE_HEIGHT, ctx.buff, OLED_FONT_SIZE_11);
    oled_update();
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

        _show_wait_connection();
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

        _show_wait_connection();
        osDelay(50);
    } while (!cmdClientIsConnected());

    menuPrintfInfo("Connected:\n%s\n Try read data", ctx.ap_name);
    change_state(STATE_GET_SERVER_DATA);
}

static void bootup_get_server_data(void)
{
    uint32_t time_to_connect = 0;
    uint32_t start_status = 0;

    while (cmdClientGetValue(MENU_START_SYSTEM, &start_status, 150) == 0)
    {
        if (time_to_connect < 5)
        {
            time_to_connect++;
        }
        else
        {
            LOG(PRINT_INFO, "Timeout get MENU_START_SYSTEM");
            change_state(STATE_EXIT);
            return;
        }
    }

    if (start_status > 0)
    {
        if (cmdClientGetAllValue(150) == 0)
        {
            LOG(PRINT_INFO, "Timeout get ALL VALUES");
            change_state(STATE_EXIT);
        }
    }

    menuPrintfInfo("Read data from: %s\n", ctx.ap_name);
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
    mainMenuInit(MENU_DRV_NORMAL_INIT);
    if (ctx.system_connected)
    {
        enterMenuStart();
    }
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
    oled_printFixed(2, 0, menu->name, OLED_FONT_SIZE_16);
    oled_setGLCDFont(OLED_FONT_SIZE_11);

    switch (ctx.state)
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

void menuInitBootupMenu(void)
{
    memset(&ctx, 0, sizeof(ctx));
    bootup_menu.menu_cb.enter = menu_enter_cb;
    bootup_menu.menu_cb.button_init_cb = menu_button_init_cb;
    bootup_menu.menu_cb.exit = menu_exit_cb;
    bootup_menu.menu_cb.process = menu_process;

    menuEnter(&bootup_menu);
}
