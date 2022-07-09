#include "config.h"
#include "menu.h"
#include "menu_drv.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "menu_default.h"
#include "menu_backend.h"
#include "freertos/timers.h"

#include "wifidrv.h"
#include "cmd_client.h"
#include "fast_add.h"
#include "battery.h"
#include "buzzer.h"
#include "start_menu.h"

#define MODULE_NAME                 "[START] "
#define DEBUG_LVL                   PRINT_DEBUG

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG(_lvl, ...) \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

#define DEVICE_LIST_SIZE            16
#define CHANGE_MENU_TIMEOUT_MS      1500
#define POWER_SAVE_TIMEOUT_MS       30 * 1000
#define CHANGE_VALUE_DISP_OFFSET    40
#define MENU_START_OFFSET           42

typedef enum
{
    STATE_INIT,
    STATE_CHECK_WIFI,
    STATE_IDLE,
    STATE_START,
    STATE_READY,
    STATE_POWER_SAVE,
    STATE_ERROR,
    STATE_INFO,
    STATE_MOTOR_CHANGE,
    STATE_SERVO_VIBRO_CHANGE,
    STATE_LOW_SILOS,
    STATE_STOP,
    STATE_ERROR_CHECK,
    STATE_RECONNECT,
    STATE_WAIT_CONNECT,
    STATE_TOP,
} state_start_menu_t;

typedef enum
{
    EDIT_MOTOR,
    EDIT_SERVO,
    EDIT_VIBRO_OFF_S,
    EDIT_VIBRO_ON_S,
    EDIT_TOP,
} edit_value_t;

typedef struct
{
    volatile state_start_menu_t state;
    state_start_menu_t last_state;
    bool error_flag;
    bool exit_wait_flag;
    int error_code;
    char *error_msg;
    char *info_msg;
    char buff[128];
    char ap_name[64];
    uint32_t timeout_con;
    uint32_t low_silos_ckeck_timeout;

    error_type_t error_dev;
    edit_value_t edit_value;
    uint32_t motor_value;
    uint32_t servo_value;
    uint32_t vibro_off_s;
    uint32_t vibro_on_s;
    bool motor_on;
    bool servo_vibro_on;

    TickType_t animation_timeout;
    uint8_t animation_cnt;
    TickType_t change_menu_timeout;
    TickType_t go_to_power_save_timeout;
    TickType_t low_silos_timeout;
    TimerHandle_t servo_timer;
} menu_start_context_t;

static menu_start_context_t ctx;

loadBar_t motor_bar =
{
    .x      = 40,
    .y      = 10,
    .width  = 80,
    .height = 10,
};

loadBar_t servo_bar =
{
    .x      = 40,
    .y      = 40,
    .width  = 80,
    .height = 10,
};

static char *state_name[] =
{
    [STATE_INIT] = "STATE_INIT",
    [STATE_IDLE] = "STATE_IDLE",
    [STATE_CHECK_WIFI] = "STATE_CHECK_WIFI",
    [STATE_START] = "STATE_START",
    [STATE_READY] = "STATE_READY",
    [STATE_POWER_SAVE] = "STATE_POWER_SAVE",
    [STATE_ERROR] = "STATE_ERROR",
    [STATE_INFO] = "STATE_INFO",
    [STATE_MOTOR_CHANGE] = "STATE_MOTOR_CHANGE",
    [STATE_SERVO_VIBRO_CHANGE] = "STATE_SERVO_VIBRO_CHANGE",
    [STATE_LOW_SILOS] = "STATE_LOW_SILOS",
    [STATE_STOP] = "STATE_STOP",
    [STATE_ERROR_CHECK] = "STATE_ERROR_CHECK",
    [STATE_RECONNECT] = "STATE_RECONNECT",
    [STATE_WAIT_CONNECT] = "STATE_WAIT_CONNECT"
};

static void change_state(state_start_menu_t new_state)
{
    debug_function_name(__func__);
    if (ctx.state < STATE_TOP)
    {
        if (ctx.state != new_state)
        {
            LOG(PRINT_INFO, "Start menu %s", state_name[new_state]);
        }

        ctx.state = new_state;
    }
    else
    {
        LOG(PRINT_INFO, "change state %d", new_state);
    }
}

static void _reset_error(void)
{
    if (menuGetValue(MENU_MACHINE_ERRORS))
    {
        cmdClientSetValueWithoutResp(MENU_MACHINE_ERRORS, 0);
    }
}

static void set_change_menu(edit_value_t val)
{
    debug_function_name(__func__);
    if ((ctx.state == STATE_READY) || (ctx.state == STATE_SERVO_VIBRO_CHANGE) || (ctx.state == STATE_MOTOR_CHANGE) ||
        (ctx.state == STATE_LOW_SILOS))
    {
        switch (val)
        {
        case EDIT_MOTOR:
            change_state(STATE_MOTOR_CHANGE);
            break;

        case EDIT_VIBRO_ON_S:
        case EDIT_SERVO:
        case EDIT_VIBRO_OFF_S:
            change_state(STATE_SERVO_VIBRO_CHANGE);
            break;

        default:
            return;
        }

        ctx.change_menu_timeout = MS2ST(CHANGE_MENU_TIMEOUT_MS) + xTaskGetTickCount();
    }
}

static bool _is_working_state(void)
{
    if ((ctx.state == STATE_READY) || (ctx.state == STATE_SERVO_VIBRO_CHANGE) || (ctx.state == STATE_MOTOR_CHANGE) ||
        (ctx.state == STATE_LOW_SILOS))
    {
        return true;
    }

    return false;
}

static bool _is_power_save(void)
{
    if (ctx.state == STATE_POWER_SAVE)
    {
        return true;
    }

    return false;
}

// static void _enter_power_save(void)
// {
//  wifiDrvPowerSave(true);
// }

static void _exit_power_save(void)
{
    wifiDrvPowerSave(false);
}

static void _reset_power_save_timer(void)
{
    ctx.go_to_power_save_timeout = MS2ST(POWER_SAVE_TIMEOUT_MS) + xTaskGetTickCount();
}

static bool _check_low_silos_flag(void)
{
    uint32_t flag = menuGetValue(MENU_LOW_LEVEL_SILOS);

    // LOG(PRINT_INFO, "------SILOS FLAG %d---------", flag);
    if (flag > 0)
    {
        if (ctx.low_silos_ckeck_timeout < xTaskGetTickCount())
        {
            ctx.low_silos_ckeck_timeout = MS2ST(30000) + xTaskGetTickCount();
            change_state(STATE_LOW_SILOS);
            buzzer_click();
            ctx.low_silos_timeout = MS2ST(5000) + xTaskGetTickCount();
            return true;
        }
    }

    return false;
}

static void menu_enter_parameters_callback(void *arg)
{
    enterMenuParameters();
}

static void menu_button_up_callback(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_error();
    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    ctx.edit_value = EDIT_VIBRO_ON_S;
}

static void menu_button_down_callback(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_error();
    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    ctx.edit_value = EDIT_VIBRO_OFF_S;
}

static void menu_button_exit_callback(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    menuExit(menu);
    ctx.exit_wait_flag = true;
}

static void menu_button_servo_callback(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_error();
    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    set_change_menu(EDIT_SERVO);
    xTimerStop(ctx.servo_timer, 0);
    ctx.servo_vibro_on = ctx.servo_vibro_on ? false : true;
    cmdClientSetValueWithoutResp(MENU_SERVO_IS_ON, ctx.servo_vibro_on);
}

static void menu_button_motor_callback(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_error();
    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    set_change_menu(EDIT_MOTOR);

    if (ctx.motor_on)
    {
        ctx.motor_on = false;
        ctx.servo_vibro_on = false;
        cmdClientSetValueWithoutResp(MENU_MOTOR_IS_ON, ctx.motor_on);
        cmdClientSetValueWithoutResp(MENU_SERVO_IS_ON, ctx.servo_vibro_on);
        xTimerStop(ctx.servo_timer, 0);
    }
    else
    {
        ctx.motor_on = true;
        xTimerStart(ctx.servo_timer, 0);
        cmdClientSetValueWithoutResp(MENU_MOTOR_IS_ON, ctx.motor_on);
    }
}

static void motor_fast_add_cb(uint32_t value)
{
    debug_function_name(__func__);
    (void)value;
    cmdClientSetValueWithoutResp(MENU_MOTOR, ctx.motor_value);
    set_change_menu(EDIT_MOTOR);
}

static void menu_button_motor_plus_push_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_error();
    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    if (ctx.motor_value < 100)
    {
        ctx.motor_value++;
        cmdClientSetValueWithoutResp(MENU_MOTOR, ctx.motor_value);
    }

    set_change_menu(EDIT_MOTOR);
}

static void menu_button_motor_plus_time_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    fastProcessStart(&ctx.motor_value, 100, 1, FP_PLUS, motor_fast_add_cb);
}

static void menu_button_motor_minus_push_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_error();
    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    if (ctx.motor_value > 1)
    {
        ctx.motor_value--;
        cmdClientSetValueWithoutResp(MENU_MOTOR, ctx.motor_value);
    }

    set_change_menu(EDIT_MOTOR);
}

static void menu_button_motor_minus_time_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    fastProcessStart(&ctx.motor_value, 100, 1, FP_MINUS, motor_fast_add_cb);
}

static void menu_button_motor_p_m_pull_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    fastProcessStop(&ctx.motor_value);

    _reset_error();
    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    menuDrvSaveParameters();
}

/*-------------SERVO BUTTONS------------*/

static void servo_fast_add_cb(uint32_t value)
{
    debug_function_name(__func__);
    (void)value;
#if CONFIG_DEVICE_SOLARKA
    if (ctx.edit_value == EDIT_VIBRO_OFF_S)
    {
        cmdClientSetValueWithoutResp(MENU_VIBRO_OFF_S, ctx.vibro_off_s);
    }
    else
    {
        cmdClientSetValueWithoutResp(MENU_VIBRO_ON_S, ctx.vibro_on_s);
    }
#elif CONFIG_DEVICE_SIEWNIK
    cmdClientSetValueWithoutResp(MENU_SERVO, ctx.servo_value);
#endif
    set_change_menu(EDIT_SERVO);
}

static void menu_button_servo_plus_push_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_error();
    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

#if CONFIG_DEVICE_SOLARKA
    if (ctx.edit_value == EDIT_VIBRO_OFF_S)
    {
        if (ctx.vibro_off_s < 100)
        {
            ctx.vibro_off_s++;
            /* vibro value change */
            cmdClientSetValueWithoutResp(MENU_VIBRO_OFF_S, ctx.vibro_off_s);
        }
    }
    else
    {
        if (ctx.vibro_on_s < 100)
        {
            ctx.vibro_on_s++;
            /* vibro value change */
            cmdClientSetValueWithoutResp(MENU_VIBRO_ON_S, ctx.vibro_on_s);
        }
    }
#elif CONFIG_DEVICE_SIEWNIK
    if (ctx.servo_value < 100)
    {
        ctx.servo_value++;
        cmdClientSetValueWithoutResp(MENU_SERVO, ctx.servo_value);
    }
#endif
    set_change_menu(EDIT_SERVO);
}

static void menu_button_servo_plus_time_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

#if CONFIG_DEVICE_SOLARKA
    if (ctx.edit_value == EDIT_VIBRO_OFF_S)
    {
        fastProcessStart(&ctx.vibro_off_s, 100, 0, FP_PLUS, servo_fast_add_cb);
    }
    else
    {
        fastProcessStart(&ctx.vibro_on_s, 100, 0, FP_PLUS, servo_fast_add_cb);
    }
#elif CONFIG_DEVICE_SIEWNIK
    fastProcessStart(&ctx.servo_value, 100, 0, FP_PLUS, servo_fast_add_cb);
#endif
}

static void menu_button_servo_minus_push_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_error();
    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

#if CONFIG_DEVICE_SOLARKA
    if (ctx.edit_value == EDIT_VIBRO_OFF_S)
    {
        if (ctx.vibro_off_s > 0)
        {
            ctx.vibro_off_s--;
            /* vibro value change */
            cmdClientSetValueWithoutResp(MENU_VIBRO_OFF_S, ctx.vibro_off_s);
        }
    }
    else
    {
        if (ctx.vibro_on_s > 0)
        {
            ctx.vibro_on_s--;
            /* vibro value change */
            cmdClientSetValueWithoutResp(MENU_VIBRO_ON_S, ctx.vibro_on_s);
        }
    }
#elif CONFIG_DEVICE_SIEWNIK
    if (ctx.servo_value > 0)
    {
        ctx.servo_value--;
        cmdClientSetValueWithoutResp(MENU_SERVO, ctx.servo_value);
    }
#endif
    set_change_menu(EDIT_SERVO);
}

static void menu_button_servo_minus_time_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

#if CONFIG_DEVICE_SOLARKA
    if (ctx.edit_value == EDIT_VIBRO_OFF_S)
    {
        fastProcessStart(&ctx.vibro_off_s, 100, 0, FP_MINUS, servo_fast_add_cb);
    }
    else
    {
        fastProcessStart(&ctx.vibro_on_s, 100, 0, FP_MINUS, servo_fast_add_cb);
    }
#elif CONFIG_DEVICE_SIEWNIK
    fastProcessStart(&ctx.servo_value, 100, 0, FP_MINUS, servo_fast_add_cb);
#endif
}

static void menu_button_servo_p_m_pull_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

#if CONFIG_DEVICE_SOLARKA
    fastProcessStop(&ctx.vibro_off_s);
    fastProcessStop(&ctx.vibro_on_s);
#elif CONFIG_DEVICE_SIEWNIK
    fastProcessStop(&ctx.servo_value);
#endif

    _reset_power_save_timer();

    if (_is_power_save())
    {
        _exit_power_save();
        change_state(STATE_READY);
    }

    if (!_is_working_state())
    {
        return;
    }

    menuDrvSaveParameters();
}

static bool menu_button_init_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return false;
    }

    menu->button.down.fall_callback = menu_button_down_callback;
    menu->button.down.timer_callback = menu_enter_parameters_callback;
    menu->button.up.timer_callback = menu_enter_parameters_callback;
    menu->button.up.fall_callback = menu_button_up_callback;
    menu->button.enter.fall_callback = menu_button_exit_callback;
    menu->button.exit.fall_callback = menu_button_servo_callback;

    menu->button.up_minus.fall_callback = menu_button_motor_minus_push_cb;
    menu->button.up_minus.rise_callback = menu_button_motor_p_m_pull_cb;
    menu->button.up_minus.timer_callback = menu_button_motor_minus_time_cb;
    menu->button.up_plus.fall_callback = menu_button_motor_plus_push_cb;
    menu->button.up_plus.rise_callback = menu_button_motor_p_m_pull_cb;
    menu->button.up_plus.timer_callback = menu_button_motor_plus_time_cb;

    menu->button.down_minus.fall_callback = menu_button_servo_minus_push_cb;
    menu->button.down_minus.rise_callback = menu_button_servo_p_m_pull_cb;
    menu->button.down_minus.timer_callback = menu_button_servo_minus_time_cb;
    menu->button.down_plus.fall_callback = menu_button_servo_plus_push_cb;
    menu->button.down_plus.rise_callback = menu_button_servo_p_m_pull_cb;
    menu->button.down_plus.timer_callback = menu_button_servo_plus_time_cb;

    menu->button.motor_on.fall_callback = menu_button_motor_callback;
    return true;
}

static bool menu_enter_cb(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return false;
    }

    _exit_power_save();
    _reset_power_save_timer();

    if (!backendIsConnected())
    {
        change_state(STATE_INIT);
    }

    cmdClientSetValueWithoutResp(MENU_START_SYSTEM, 1);

    ctx.error_flag = 0;
    return true;
}

static bool menu_exit_cb(void *arg)
{
    debug_function_name(__func__);

    backendExitMenuStart();

    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return false;
    }

    MOTOR_LED_SET_GREEN(0);
    SERVO_VIBRO_LED_SET_GREEN(0);
    return true;
}

static void menu_set_error_msg(char *msg)
{
    ctx.error_msg = msg;
    ctx.error_flag = 1;
    change_state(STATE_ERROR_CHECK);
}

static void menu_start_init(void)
{
    ssd1306_SetCursor(2, MENU_HEIGHT + 2 * LINE_HEIGHT);
    ssd1306_WriteString("Check connection", Font_7x10, White);
    ssd1306_UpdateScreen();
    change_state(STATE_CHECK_WIFI);
}

static void menu_check_connection(void)
{
    if (!backendIsConnected())
    {
        change_state(STATE_RECONNECT);
        return;
    }

    bool ret = false;

    for (uint8_t i = 0; i < 3; i++)
    {
        LOG(PRINT_INFO, "START_MENU: cmdClientGetAllValue try %d", i);
        osDelay(250);
        if ((cmdClientSetValue(MENU_MOTOR_IS_ON, 0, 1000) == TRUE) &&
            (cmdClientSetValue(MENU_SERVO_IS_ON, 0, 1000) == TRUE) &&
            (cmdClientSetValue(MENU_MOTOR, menuGetValue(MENU_MOTOR),
            1000) == TRUE) && (cmdClientSetValue(MENU_SERVO, menuGetValue(MENU_SERVO), 1000) == TRUE))
        {
            ctx.motor_value = menuGetValue(MENU_MOTOR);
            ctx.servo_value = menuGetValue(MENU_SERVO);
            ctx.motor_on = menuGetValue(MENU_MOTOR_IS_ON);
            ctx.servo_vibro_on = menuGetValue(MENU_SERVO_IS_ON);

            cmdClientSetValueWithoutResp(MENU_EMERGENCY_DISABLE, 0);
            ret = true;
            break;
        }
        else
        {
            sprintf(ctx.buff, "Check connection %s%s%s", ".", i > 0 ? "." : " ", i > 1 ? "." : " ");
            ssd1306_SetCursor(2, MENU_HEIGHT + 2 * LINE_HEIGHT);
            ssd1306_WriteString(ctx.buff, Font_7x10, White);
            ssd1306_UpdateScreen();
        }
    }

    if (ret != TRUE)
    {
        LOG(PRINT_INFO, "%s: error get parameters", __func__);
        cmdClientSetValueWithoutResp(MENU_MOTOR_IS_ON, 0);
        cmdClientSetValueWithoutResp(MENU_SERVO_IS_ON, 0);
        change_state(STATE_IDLE);
    }

    change_state(STATE_IDLE);
}

static void menu_start_idle(void)
{
    if (backendIsConnected())
    {
        cmdClientSetValueWithoutResp(MENU_START_SYSTEM, 1);
        change_state(STATE_START);
    }
    else
    {
        menuPrintfInfo("Target not connected.\nGo to DEVICES\nfor connect");
    }
}

static void menu_start_start(void)
{
    if (!backendIsConnected())
    {
        return;
    }

    change_state(STATE_READY);
}

static void menu_start_ready(void)
{
    debug_function_name(__func__);
    if (!backendIsConnected())
    {
        menu_set_error_msg("Lost connection with server");
        return;
    }

    if (_check_low_silos_flag())
    {
        return;
    }

/* Enter power save. Not used */
#if 0
    if (ctx.go_to_power_save_timeout < xTaskGetTickCount())
    {
        _enter_power_save();
        change_state(STATE_POWER_SAVE);
        return;
    }
#endif

    if (ctx.animation_timeout < xTaskGetTickCount())
    {
        ctx.animation_cnt++;
        ctx.animation_timeout = xTaskGetTickCount() + MS2ST(100);
    }

    char str[8];

    motor_bar.fill = ctx.motor_value;
    sprintf(str, "%d%%", motor_bar.fill);
    ssd1306_Fill(Black);
    ssdFigureDrawLoadBar(&motor_bar);
    ssd1306_SetCursor(80, 25);
    ssd1306_WriteString(str, Font_7x10, White);
    uint8_t cnt = 0;

    if (ctx.motor_on)
    {
        cnt = ctx.animation_cnt % 8;
    }

    if (cnt < 4)
    {
        if (cnt < 2)
        {
            drawMotor(2, 2 - cnt);
        }
        else
        {
            drawMotor(2, cnt - 2);
        }
    }
    else
    {
        if (cnt < 6)
        {
            drawMotor(2, cnt - 2);
        }
        else
        {
            drawMotor(2, 10 - cnt);
        }
    }

#if CONFIG_DEVICE_SOLARKA
    /* PERIOD CURSOR */
    char menu_buff[32];

    ssd1306_SetCursor(2, MENU_START_OFFSET);
    sprintf(menu_buff, "Vibro ON: %d [s]", ctx.vibro_on_s);

    if (ctx.edit_value == EDIT_VIBRO_ON_S)
    {
        ssdFigureFillLine(MENU_START_OFFSET, LINE_HEIGHT);
        ssd1306_WriteString(menu_buff, Font_7x10, Black);
    }
    else
    {
        ssd1306_WriteString(menu_buff, Font_7x10, White);
    }

    /* WORKING TIME CURSOR */
    sprintf(menu_buff, "Vibro OFF: %d [s]", ctx.vibro_off_s);
    ssd1306_SetCursor(2, MENU_START_OFFSET + LINE_HEIGHT);
    if (ctx.edit_value == EDIT_VIBRO_OFF_S)
    {
        ssdFigureFillLine(MENU_START_OFFSET + LINE_HEIGHT, LINE_HEIGHT);
        ssd1306_WriteString(menu_buff, Font_7x10, Black);
    }
    else
    {
        ssd1306_WriteString(menu_buff, Font_7x10, White);
    }
#elif CONFIG_DEVICE_SIEWNIK
    servo_bar.fill = ctx.servo_value;
    sprintf(str, "%d", servo_bar.fill);
    ssdFigureDrawLoadBar(&servo_bar);
    ssd1306_SetCursor(80, 55);
    ssd1306_WriteString(str, Font_7x10, White);
    if (ctx.servo_vibro_on)
    {
        drawServo(10, 35, ctx.servo_value);
    }
    else
    {
        drawServo(10, 35, 0);
    }
#endif
    backendEnterMenuStart();
}

static void menu_start_power_save(void)
{
    if (!backendIsConnected())
    {
        menu_set_error_msg("Lost connection with server");
        return;
    }

    if (_check_low_silos_flag())
    {
        return;
    }

    menuPrintfInfo("Power save");
    backendEnterMenuStart();
}

static void menu_start_low_silos(void)
{
    if (!backendIsConnected())
    {
        menu_set_error_msg("Lost connection with server");
        return;
    }

    if (ctx.low_silos_timeout < xTaskGetTickCount())
    {
        change_state(STATE_READY);
        return;
    }

    ssd1306_Fill(Black);
    ssd1306_SetCursor(2, 2);
    ssd1306_WriteString("Low", Font_16x26, White);
    ssd1306_SetCursor(2, 2 + 28);
    ssd1306_WriteString("silos", Font_16x26, White);
}

static void menu_start_error(void)
{
    if (!backendIsConnected())
    {
        menu_set_error_msg("Lost connection with server");
        return;
    }

    switch (ctx.error_dev)
    {
#if CONFIG_DEVICE_SIEWNIK
    case ERROR_SERVO_NOT_CONNECTED:
        menuPrintfInfo("Servo not\nconnected.\nClick any button\nto reset error");
        break;

    case ERROR_SERVO_OVER_CURRENT:
        menuPrintfInfo("Servo overcurrent.\nClick any button\nto reset error");
        break;
#endif
    case ERROR_MOTOR_NOT_CONNECTED:
        menuPrintfInfo("Motor not\nconnected.\nClick any button\nto reset error");
        break;

    case ERROR_MOTOR_OVER_CURRENT:
        menuPrintfInfo("Motor overcurrent.\nClick any button\nto reset error");
        break;

    case ERROR_OVER_TEMPERATURE:
        menuPrintfInfo("Temperature is\nhigh.\nClick any button\nto reset error");
        break;

    default:
        menuPrintfInfo("Unknown error.\nClick any button\nto reset error");
        break;
    }
}

static void menu_start_info(void)
{
    debug_function_name(__func__);
    osDelay(2500);
    change_state(ctx.last_state);
}

static void menu_start_motor_change(void)
{
    if (!backendIsConnected())
    {
        menu_set_error_msg("Lost connection with server");
        return;
    }

    ssd1306_SetCursor(2, 0);
    ssd1306_WriteString("   MOTOR", Font_11x18, White);
    ssd1306_SetCursor(CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT);
    sprintf(ctx.buff, "%d%%", ctx.motor_value);
    ssd1306_WriteString(ctx.buff, Font_16x26, White);

    if (ctx.change_menu_timeout < xTaskGetTickCount())
    {
        change_state(STATE_READY);
    }
}

static void menu_start_vibro_change(void)
{
    debug_function_name(__func__);
    if (!backendIsConnected())
    {
        menu_set_error_msg("Lost connection with server");
        return;
    }

#if CONFIG_DEVICE_SIEWNIK
    ssd1306_SetCursor(2, 0);
    ssd1306_WriteString("   SERVO", Font_11x18, White);
    ssd1306_SetCursor(CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT);
    sprintf(ctx.buff, "%d%%", ctx.servo_value);
    ssd1306_WriteString(ctx.buff, Font_16x26, White);
#endif

#if CONFIG_DEVICE_SOLARKA
    ssd1306_SetCursor(2, 0);
    ssd1306_WriteString(ctx.edit_value == EDIT_VIBRO_OFF_S ? "Vibro OFF" : "Vibro ON", Font_11x18, White);
    ssd1306_SetCursor(CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT);
    sprintf(ctx.buff, "%d [s]", ctx.edit_value == EDIT_VIBRO_OFF_S ? ctx.vibro_off_s : ctx.vibro_on_s);
    ssd1306_WriteString(ctx.buff, Font_16x26, White);
#endif

    if (ctx.change_menu_timeout < xTaskGetTickCount())
    {
        change_state(STATE_READY);
    }
}

static void menu_start_stop(void)
{
}

static void menu_start_error_check(void)
{
    if (ctx.error_flag)
    {
        menuPrintfInfo(ctx.error_msg);
        ctx.error_flag = false;
    }
    else
    {
        change_state(STATE_INIT);
        osDelay(700);
    }
}

static void menu_reconnect(void)
{
    backendExitMenuStart();

    wifiDrvGetAPName(ctx.ap_name);
    if (strlen(ctx.ap_name) > 5)
    {
        wifiDrvConnect();
        change_state(STATE_WAIT_CONNECT);
    }
}

static void _show_wait_connection(void)
{
    sprintf(ctx.buff, "Wait connection%s%s%s", xTaskGetTickCount() % 400 > 100 ? "." : " ",
        xTaskGetTickCount() % 400 > 200 ? "." : " ", xTaskGetTickCount() % 400 > 300 ? "." : " ");
    ssd1306_SetCursor(2, MENU_HEIGHT + 2 * LINE_HEIGHT);
    ssd1306_WriteString(ctx.buff, Font_7x10, White);
    ssd1306_UpdateScreen();
}

static void menu_wait_connect(void)
{
    /* Wait to connect wifi */
    ctx.timeout_con = MS2ST(10000) + xTaskGetTickCount();
    ctx.exit_wait_flag = false;
    do
    {
        if ((ctx.timeout_con < xTaskGetTickCount()) || ctx.exit_wait_flag)
        {
            menu_set_error_msg("Timeout connect");
            return;
        }

        _show_wait_connection();
        osDelay(50);
    } while (wifiDrvTryingConnect());

    ctx.timeout_con = MS2ST(10000) + xTaskGetTickCount();
    do
    {
        if ((ctx.timeout_con < xTaskGetTickCount()) || ctx.exit_wait_flag)
        {
            menu_set_error_msg("Timeout server");
            return;
        }

        _show_wait_connection();
        osDelay(50);
    } while (!cmdClientIsConnected());

    menuPrintfInfo("Connected:\n%s\nTry read data   ", ctx.ap_name);
    change_state(STATE_CHECK_WIFI);
}

static bool menu_process(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return false;
    }

    switch (ctx.state)
    {
    case STATE_INIT:
        menu_start_init();
        break;

    case STATE_CHECK_WIFI:
        menu_check_connection();
        break;

    case STATE_IDLE:
        menu_start_idle();
        break;

    case STATE_START:
        menu_start_start();
        break;

    case STATE_READY:
        menu_start_ready();
        break;

    case STATE_POWER_SAVE:
        menu_start_power_save();
        break;

    case STATE_ERROR:
        menu_start_error();
        break;

    case STATE_INFO:
        menu_start_info();
        break;

    case STATE_MOTOR_CHANGE:
        menu_start_motor_change();
        break;

    case STATE_SERVO_VIBRO_CHANGE:
        menu_start_vibro_change();
        break;

    case STATE_LOW_SILOS:
        menu_start_low_silos();
        break;

    case STATE_STOP:
        menu_start_stop();
        break;

    case STATE_ERROR_CHECK:
        menu_start_error_check();
        break;

    case STATE_RECONNECT:
        menu_reconnect();
        break;

    case STATE_WAIT_CONNECT:
        menu_wait_connect();
        break;

    default:
        change_state(STATE_STOP);
        break;
    }

    if (!backendIsEmergencyDisable())
    {
        MOTOR_LED_SET_GREEN(ctx.motor_on);
        SERVO_VIBRO_LED_SET_GREEN(ctx.servo_vibro_on);
    }
    else
    {
        MOTOR_LED_SET_GREEN(0);
        SERVO_VIBRO_LED_SET_GREEN(0);
        xTimerStop(ctx.servo_timer, 0);
    }

    return true;
}

static void timerCallback(void *pv)
{
    ctx.servo_vibro_on = true;
    cmdClientSetValueWithoutResp(MENU_SERVO_IS_ON, ctx.servo_vibro_on);
}

void menuStartReset(void)
{
    ctx.motor_on = false;
    ctx.servo_vibro_on = false;
}

void menuInitStartMenu(menu_token_t *menu)
{
    memset(&ctx, 0, sizeof(ctx));
    ctx.edit_value = EDIT_VIBRO_ON_S;
    menu->menu_cb.enter = menu_enter_cb;
    menu->menu_cb.button_init_cb = menu_button_init_cb;
    menu->menu_cb.exit = menu_exit_cb;
    menu->menu_cb.process = menu_process;
    ctx.servo_timer = xTimerCreate("servoTimer", MS2ST(2000), pdFALSE, (void *)0, timerCallback);
}

void menuStartSetError(error_type_t error)
{
    LOG(PRINT_DEBUG, "%s %d", __func__, error);
    ctx.error_dev = error;
    change_state(STATE_ERROR);
}

void menuStartResetError(void)
{
    LOG(PRINT_DEBUG, "%s", __func__);
    if (ctx.state == STATE_ERROR)
    {
        ctx.error_dev = ERROR_TOP;
        change_state(STATE_READY);
    }
}
