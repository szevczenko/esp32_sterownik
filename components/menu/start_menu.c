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
#include "oled.h"

#define MODULE_NAME                 "[START] "
#define DEBUG_LVL                   PRINT_INFO

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
#if MENU_VIRO_ON_OFF_VERSION
    EDIT_VIBRO_OFF_S,
    EDIT_VIBRO_ON_S,
#endif
    EDIT_TOP,
} edit_value_t;

typedef struct
{
    volatile state_start_menu_t state;
    state_start_menu_t last_state;
    bool error_flag;
    bool exit_wait_flag;
    int error_code;
    const char *error_msg;
    const char *info_msg;
    char buff[128];
    char ap_name[64];
    uint32_t timeout_con;
    uint32_t low_silos_ckeck_timeout;
    error_type_t error_dev;
    
#if MENU_VIRO_ON_OFF_VERSION
    edit_value_t edit_value;
#endif

    struct menu_data data;
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
#if MENU_VIRO_ON_OFF_VERSION
        case EDIT_VIBRO_ON_S:
        case EDIT_VIBRO_OFF_S:
#endif
        case EDIT_SERVO:
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

#if MENU_VIRO_ON_OFF_VERSION
    ctx.edit_value = EDIT_VIBRO_ON_S;
#endif
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
#if MENU_VIRO_ON_OFF_VERSION
    ctx.edit_value = EDIT_VIBRO_OFF_S;
#endif
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
    _reset_error();
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
    ctx.data.servo_vibro_on = ctx.data.servo_vibro_on ? false : true;
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

    if (ctx.data.motor_on)
    {
        ctx.data.motor_on = false;
        ctx.data.servo_vibro_on = false;
        xTimerStop(ctx.servo_timer, 0);
    }
    else
    {
        ctx.data.motor_on = true;
        xTimerStart(ctx.servo_timer, 0);
    }
}

static void motor_fast_add_cb(uint32_t value)
{
    debug_function_name(__func__);
    (void)value;
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

    if (ctx.data.motor_value < 100)
    {
        ctx.data.motor_value++;
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

    fastProcessStart(&ctx.data.motor_value, 100, 1, FP_PLUS, motor_fast_add_cb);
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

    if (ctx.data.motor_value > 1)
    {
        ctx.data.motor_value--;
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

    fastProcessStart(&ctx.data.motor_value, 100, 1, FP_MINUS, motor_fast_add_cb);
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

    fastProcessStop(&ctx.data.motor_value);

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
#if MENU_VIRO_ON_OFF_VERSION
    if (config.dev_type == T_DEV_TYPE_SOLARKA)
    {
        if (ctx.edit_value == EDIT_VIBRO_OFF_S)
        {
            if (ctx.data.vibro_off_s < 100)
            {
                ctx.data.vibro_off_s++;
            }
        }
        else
        {
            if (ctx.data.vibro_on_s < 100)
            {
                ctx.data.vibro_on_s++;
            }
        }
    }
    else
#endif
    {
        if (ctx.data.servo_value < 100)
        {
            ctx.data.servo_value++;
        }
    }
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
#if MENU_VIRO_ON_OFF_VERSION
    if (config.dev_type == T_DEV_TYPE_SOLARKA)
    {
        if (ctx.edit_value == EDIT_VIBRO_OFF_S)
        {
            fastProcessStart(&ctx.data.vibro_off_s, 100, 0, FP_PLUS, servo_fast_add_cb);
        }
        else
        {
            fastProcessStart(&ctx.data.vibro_on_s, 100, 1, FP_PLUS, servo_fast_add_cb);
        }
    }
    else
#endif
    {
        fastProcessStart(&ctx.data.servo_value, 100, 0, FP_PLUS, servo_fast_add_cb);
    }
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
#if MENU_VIRO_ON_OFF_VERSION
    if (config.dev_type == T_DEV_TYPE_SOLARKA)
    {
        if (ctx.edit_value == EDIT_VIBRO_OFF_S)
        {
            if (ctx.data.vibro_off_s > 0)
            {
                ctx.data.vibro_off_s--;
            }
        }
        else
        {
            if (ctx.data.vibro_on_s > 1)
            {
                ctx.data.vibro_on_s--;
            }
        }
    }
    else
#endif
    {
        if (ctx.data.servo_value > 0)
        {
            ctx.data.servo_value--;
        }
    }
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
#if MENU_VIRO_ON_OFF_VERSION
    if (config.dev_type == T_DEV_TYPE_SOLARKA)
    {
        if (ctx.edit_value == EDIT_VIBRO_OFF_S)
        {
            fastProcessStart(&ctx.data.vibro_off_s, 100, 0, FP_MINUS, servo_fast_add_cb);
        }
        else
        {
            fastProcessStart(&ctx.data.vibro_on_s, 100, 1, FP_MINUS, servo_fast_add_cb);
        }
    }
    else
#endif
    {
        fastProcessStart(&ctx.data.servo_value, 100, 0, FP_MINUS, servo_fast_add_cb);
    }
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
#if MENU_VIRO_ON_OFF_VERSION
    if (config.dev_type == T_DEV_TYPE_SOLARKA)
    {
        fastProcessStop(&ctx.data.vibro_off_s);
        fastProcessStop(&ctx.data.vibro_on_s);
    }
    else
#endif
    {
        fastProcessStop(&ctx.data.servo_value);
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

    menuDrvSaveParameters();
}

static void menu_button_on_off(void *arg)
{
    debug_function_name(__func__);
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    _reset_error();
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
    menu->button.on_off.fall_callback = menu_button_on_off;
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
    cmdClientSetValueWithoutResp(MENU_ERROR_MOTOR, menuGetValue(MENU_ERROR_MOTOR));
    cmdClientSetValueWithoutResp(MENU_ERROR_SERVO, menuGetValue(MENU_ERROR_SERVO));
    cmdClientSetValueWithoutResp(MENU_ERROR_MOTOR_CALIBRATION, menuGetValue(MENU_ERROR_MOTOR_CALIBRATION));
    backendEnterMenuStart();

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

static void menu_set_error_msg(const char *msg)
{
    ctx.error_msg = msg;
    ctx.error_flag = 1;
    change_state(STATE_ERROR_CHECK);
}

static void menu_start_init(void)
{
    oled_printFixed(2, 2 * LINE_HEIGHT, dictionary_get_string(DICT_CHECK_CONNECTION), OLED_FONT_SIZE_11);
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

    ctx.data.motor_value = menuGetValue(MENU_MOTOR);
    ctx.data.servo_value = menuGetValue(MENU_SERVO);
    ctx.data.motor_on = menuGetValue(MENU_MOTOR_IS_ON);
    ctx.data.servo_vibro_on = menuGetValue(MENU_SERVO_IS_ON);
#if MENU_VIRO_ON_OFF_VERSION
    if (menuGetValue(MENU_VIBRO_ON_S) == 0)
    {
        menuSetValue(MENU_VIBRO_ON_S, 1);
    }
    ctx.data.vibro_on_s = menuGetValue(MENU_VIBRO_ON_S);
    ctx.data.vibro_off_s = menuGetValue(MENU_VIBRO_OFF_S);
#endif
    for (uint8_t i = 0; i < 3; i++)
    {
        LOG(PRINT_INFO, "START_MENU: cmdClientGetAllValue try %d", i);
        osDelay(250);
        

        if ((cmdClientSetValue(MENU_EMERGENCY_DISABLE, 0, 1000) == TRUE) 
            && (cmdClientSetValue(MENU_PERIOD, menuGetValue(MENU_PERIOD), 1000) == TRUE))
        {
            break;
        }
    }

    if (ret != TRUE)
    {
        LOG(PRINT_INFO, "%s: error get parameters", __func__);
        ctx.data.motor_on = 0;
        ctx.data.servo_vibro_on = 0;
        change_state(STATE_IDLE);
    }

    change_state(STATE_IDLE);
}

static void menu_start_idle(void)
{
    if (backendIsConnected())
    {
        cmdClientSetValueWithoutResp(MENU_START_SYSTEM, 1);
        cmdClientSetValueWithoutResp(MENU_ERROR_MOTOR, menuGetValue(MENU_ERROR_MOTOR));
        cmdClientSetValueWithoutResp(MENU_ERROR_SERVO, menuGetValue(MENU_ERROR_SERVO));
        cmdClientSetValueWithoutResp(MENU_ERROR_MOTOR_CALIBRATION, menuGetValue(MENU_ERROR_MOTOR_CALIBRATION));
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
        menu_set_error_msg(dictionary_get_string(DICT_LOST_CONNECTION_WITH_SERVER));
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

    char str[32];

    motor_bar.fill = ctx.data.motor_value;
    sprintf(str, "%d%%", motor_bar.fill);
    oled_clearScreen();
    ssdFigureDrawLoadBar(&motor_bar);
    oled_printFixed(70, 22, str, OLED_FONT_SIZE_11);
    uint8_t cnt = 0;

    if (ctx.data.motor_on)
    {
        cnt = ctx.animation_cnt % 6;
    }

    drawMotorCircle(5, 2, cnt);

    // if (cnt < 4)
    // {
    //     if (cnt < 2)
    //     {
    //         drawMotor(2, 2 - cnt);
    //     }
    //     else
    //     {
    //         drawMotor(2, cnt - 2);
    //     }
    // }
    // else
    // {
    //     if (cnt < 6)
    //     {
    //         drawMotor(2, cnt - 2);
    //     }
    //     else
    //     {
    //         drawMotor(2, 10 - cnt);
    //     }
    // }

    if (config.dev_type == T_DEV_TYPE_SOLARKA)
    {
#if MENU_VIRO_ON_OFF_VERSION
        /* PERIOD CURSOR */
        char menu_buff[32];

        sprintf(menu_buff, "%s: %d [s]", dictionary_get_string(DICT_VIBRO_ON), ctx.data.vibro_on_s);

        if (ctx.edit_value == EDIT_VIBRO_ON_S)
        {
            ssdFigureFillLine(MENU_START_OFFSET, LINE_HEIGHT);
            oled_printFixedBlack(2, MENU_START_OFFSET, menu_buff, OLED_FONT_SIZE_11);
        }
        else
        {
            oled_printFixed(2, MENU_START_OFFSET, menu_buff, OLED_FONT_SIZE_11);
        }

        /* WORKING TIME CURSOR */
        sprintf(menu_buff, "%s: %d [s]", dictionary_get_string(DICT_VIBRO_OFF), ctx.data.vibro_off_s);
        if (ctx.edit_value == EDIT_VIBRO_OFF_S)
        {
            ssdFigureFillLine(MENU_START_OFFSET + LINE_HEIGHT, LINE_HEIGHT);
            oled_printFixedBlack(2, MENU_START_OFFSET + LINE_HEIGHT, menu_buff, OLED_FONT_SIZE_11);
        }
        else
        {
            oled_printFixed(2, MENU_START_OFFSET + LINE_HEIGHT, menu_buff, OLED_FONT_SIZE_11);
        }
#else
        if (ctx.data.servo_vibro_on)
        {
            cnt = ctx.animation_cnt % 4;
        }
        else
        {
            cnt = 0;
        }

        drawVibro(0, 33, cnt);
        servo_bar.fill = ctx.data.servo_value;
        sprintf(str, "%d%%", servo_bar.fill);
        ssdFigureDrawLoadBar(&servo_bar);
        oled_printFixed(70, 52, str, OLED_FONT_SIZE_11);
#endif
    }
    else
    {
        servo_bar.fill = ctx.data.servo_value;
        sprintf(str, "%d", servo_bar.fill);
        ssdFigureDrawLoadBar(&servo_bar);
        oled_printFixed(80, 52, str, OLED_FONT_SIZE_11);
        if (ctx.data.servo_vibro_on)
        {
            drawServo(10, 35, ctx.data.servo_value);
        }
        else
        {
            drawServo(10, 35, 0);
        }
    }
    backendEnterMenuStart();
}

static void menu_start_power_save(void)
{
    if (!backendIsConnected())
    {
        menu_set_error_msg(dictionary_get_string(DICT_LOST_CONNECTION_WITH_SERVER));
        return;
    }

    if (_check_low_silos_flag())
    {
        return;
    }

    menuPrintfInfo(dictionary_get_string(DICT_POWER_SAVE));
    backendEnterMenuStart();
}

static void menu_start_low_silos(void)
{
    if (!backendIsConnected())
    {
        menu_set_error_msg(dictionary_get_string(DICT_LOST_CONNECTION_WITH_SERVER));
        return;
    }

    if (ctx.low_silos_timeout < xTaskGetTickCount())
    {
        change_state(STATE_READY);
        return;
    }

    oled_clearScreen();
    oled_printFixed(2, 2, dictionary_get_string(DICT_LOW), OLED_FONT_SIZE_26);
    oled_printFixed(2, 32, dictionary_get_string(DICT_SILOS), OLED_FONT_SIZE_26);
}

static void menu_start_error(void)
{
    static uint32_t blink_counter;
    static bool blink_state;
    bool motor_led_blink = false;
    bool servo_led_blink = false;

    MOTOR_LED_SET_GREEN(0);
    SERVO_VIBRO_LED_SET_GREEN(0);

    if (!backendIsConnected())
    {
        menu_set_error_msg(dictionary_get_string(DICT_LOST_CONNECTION_WITH_SERVER));
        return;
    }

    switch (ctx.error_dev)
    {
    
    case ERROR_SERVO_NOT_CONNECTED:
        oled_printFixed(2, MENU_HEIGHT, dictionary_get_string(DICT_SERVO_NOT_CONNECTED), OLED_FONT_SIZE_16);
        break;

    case ERROR_SERVO_OVER_CURRENT:
        oled_printFixed(2, MENU_HEIGHT, dictionary_get_string(DICT_SERVO_OVERCURRENT), OLED_FONT_SIZE_16);
        break;

    case ERROR_MOTOR_NOT_CONNECTED:
        oled_printFixed(2, MENU_HEIGHT, dictionary_get_string(DICT_MOTOR_NOT_CONNECTED), OLED_FONT_SIZE_16);
        motor_led_blink = true;
        break;

    case ERROR_VIBRO_NOT_CONNECTED:
        oled_printFixed(2, MENU_HEIGHT, dictionary_get_string(DICT_VIBRO_NOT_CONNECTED), OLED_FONT_SIZE_16);
        servo_led_blink = true;
        break;

    case ERROR_VIBRO_OVER_CURRENT:
        oled_printFixed(2, MENU_HEIGHT, dictionary_get_string(DICT_VIBRO_OVERCURRENT), OLED_FONT_SIZE_16);
        servo_led_blink = true;
        break;

    case ERROR_MOTOR_OVER_CURRENT:
        oled_printFixed(2, MENU_HEIGHT, dictionary_get_string(DICT_MOTOR_OVERCURRENT), OLED_FONT_SIZE_16);
        motor_led_blink = true;
        break;

    case ERROR_OVER_TEMPERATURE:
        menuPrintfInfo(dictionary_get_string(DICT_TEMPERATURE_IS_HIGH));
        motor_led_blink = true;
        servo_led_blink = true;
        break;

    default:
        menuPrintfInfo(dictionary_get_string(DICT_UNKNOWN_ERROR));
        motor_led_blink = true;
        servo_led_blink = true;
        break;
    }

    if ((blink_counter++) % 2 == 0)
    {
        MOTOR_LED_SET_RED(motor_led_blink ? blink_state : 0);
        SERVO_VIBRO_LED_SET_RED(servo_led_blink ? blink_state : 0);
        blink_state = blink_state ? false : true;
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
        menu_set_error_msg(dictionary_get_string(DICT_LOST_CONNECTION_WITH_SERVER));
        return;
    }

    oled_printFixed(0, 0, dictionary_get_string(DICT_MOTOR), OLED_FONT_SIZE_26);
    sprintf(ctx.buff, "%d%%", ctx.data.motor_value);
    oled_printFixed(CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT, ctx.buff, OLED_FONT_SIZE_26); //Font_16x26

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
        menu_set_error_msg(dictionary_get_string(DICT_LOST_CONNECTION_WITH_SERVER));
        return;
    }

    if (config.dev_type == T_DEV_TYPE_SOLARKA)
    {
#if MENU_VIRO_ON_OFF_VERSION
        oled_printFixed(0, 0, dictionary_get_string(ctx.edit_value == EDIT_VIBRO_OFF_S ? DICT_VIBRO_OFF : DICT_VIBRO_OFF), OLED_FONT_SIZE_16);
        uint32_t x = 0;
        uint32_t y = MENU_HEIGHT + LINE_HEIGHT;
        if ((ctx.edit_value == EDIT_VIBRO_OFF_S ? ctx.data.vibro_off_s : ctx.data.vibro_on_s) < 10)
        {
            x = CHANGE_VALUE_DISP_OFFSET;
        }
        else if ((ctx.edit_value == EDIT_VIBRO_OFF_S ? ctx.data.vibro_off_s : ctx.data.vibro_on_s) < 100)
        {
            x = CHANGE_VALUE_DISP_OFFSET - 16;
        }
        else
        {
            x = CHANGE_VALUE_DISP_OFFSET - 32;
        }
        sprintf(ctx.buff, "%d [s]", ctx.edit_value == EDIT_VIBRO_OFF_S ? ctx.data.vibro_off_s : ctx.data.vibro_on_s);
        oled_printFixed(x, y, ctx.buff, OLED_FONT_SIZE_26);
#else
        oled_printFixed(0, 0, dictionary_get_string(DICT_VIBRO_ON), OLED_FONT_SIZE_26);
        sprintf(ctx.buff, "%d%%", ctx.data.servo_value);
        oled_printFixed(CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT, ctx.buff, OLED_FONT_SIZE_26);
#endif
    }
    else
    {
        oled_printFixed(0, 0, dictionary_get_string(DICT_SERVO), OLED_FONT_SIZE_26);
        sprintf(ctx.buff, "%d%%", ctx.data.servo_value);
        oled_printFixed(CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT, ctx.buff, OLED_FONT_SIZE_26);
    }

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
    oled_clearScreen();
    sprintf(ctx.buff, dictionary_get_string(DICT_WAIT_CONNECTION_S_S_S), xTaskGetTickCount() % 400 > 100 ? "." : " ",
        xTaskGetTickCount() % 400 > 200 ? "." : " ", xTaskGetTickCount() % 400 > 300 ? "." : " ");
    oled_printFixed(2, 2 * LINE_HEIGHT, ctx.buff, OLED_FONT_SIZE_11);
    oled_update();
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
            menu_set_error_msg(dictionary_get_string(DICT_TIMEOUT_CONNECT));
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
            menu_set_error_msg(dictionary_get_string(DICT_TIMEOUT_SERVER));
            return;
        }

        _show_wait_connection();
        osDelay(50);
    } while (!cmdClientIsConnected());

    oled_clearScreen();
    menuPrintfInfo(dictionary_get_string(DICT_CONNECTED_TRY_READ_DATA));
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

    if (backendIsEmergencyDisable() || ctx.state == STATE_ERROR || !backendIsConnected())
    {
        MOTOR_LED_SET_GREEN(0);
        SERVO_VIBRO_LED_SET_GREEN(0);
        xTimerStop(ctx.servo_timer, 0);
    }
    else
    {
        MOTOR_LED_SET_GREEN(ctx.data.motor_on);
        SERVO_VIBRO_LED_SET_GREEN(ctx.data.servo_vibro_on);
        MOTOR_LED_SET_RED(0);
        SERVO_VIBRO_LED_SET_RED(0);
    }

    return true;
}

static void timerCallback(void *pv)
{
    printf("%s\n\r", __func__);
    if (backendIsEmergencyDisable() || ctx.state == STATE_ERROR)
    {
        return;
    }

    ctx.data.servo_vibro_on = true;
}

void menuStartReset(void)
{
    ctx.data.motor_on = false;
    ctx.data.servo_vibro_on = false;
}

void menuInitStartMenu(menu_token_t *menu)
{
    memset(&ctx, 0, sizeof(ctx));
#if MENU_VIRO_ON_OFF_VERSION
    ctx.edit_value = EDIT_VIBRO_ON_S;
#endif
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
    ctx.data.motor_on = false;
    ctx.data.servo_vibro_on = false;
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

struct menu_data *menuStartGetData(void)
{
    return &ctx.data;
}