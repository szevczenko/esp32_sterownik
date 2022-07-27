#include <stdint.h>
#include "measure.h"
#include "error_solarka.h"
#include "motor.h"
#include "menu.h"
#include "servo.h"
#include "math.h"
#include "menu_param.h"
#include "vibro.h"

#include "cmd_server.h"
#include "server_controller.h"

#if CONFIG_DEVICE_SOLARKA
#define MODULE_NAME    "[Err_sola] "
#define DEBUG_LVL      PRINT_INFO

#if CONFIG_DEBUG_ERROR_SIEWNIK
#define LOG(_lvl, ...) \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

typedef enum
{
    STATE_INIT,
    STATE_IDLE,
    STATE_WORKING,
    STATE_ERROR_TEMPERATURE,
    STATE_ERROR_MOTOR_CURRENT,
    STATE_ERROR_MOTOR_NOT_CONNECTED,
    STATE_ERROR_VIBRO_NOT_CONNECTED,
    STATE_ERROR_VIBRO,
    STATE_WAIT_RESET_ERROR,
    STATE_TOP,
} state_t;

struct error_siewnik_ctx
{
    state_t state;
    TickType_t motor_error_timer;
    bool motor_find_overcurrent;

    TickType_t motor_not_connected_timer;
    TickType_t vibro_not_connected_timer;
    bool motor_find_not_connected;
    bool vibro_find_not_connected;

    bool is_error_reset;
};

static struct error_siewnik_ctx ctx;

static char *state_name[] =
{
    [STATE_INIT] = "STATE_INIT",
    [STATE_IDLE] = "STATE_IDLE",
    [STATE_WORKING] = "STATE_WORKING",
    [STATE_ERROR_TEMPERATURE] = "STATE_ERROR_TEMPERATURE",
    [STATE_ERROR_MOTOR_CURRENT] = "STATE_ERROR_MOTOR_CURRENT",
    [STATE_ERROR_MOTOR_NOT_CONNECTED] = "STATE_ERROR_MOTOR_NOT_CONNECTED",
    [STATE_ERROR_VIBRO_NOT_CONNECTED] = "STATE_ERROR_VIBRO_NOT_CONNECTED",
    [STATE_ERROR_VIBRO] = "STATE_ERROR_VIBRO",
    [STATE_WAIT_RESET_ERROR] = "STATE_WAIT_RESET_ERROR",
};

static void _change_state(state_t new_state)
{
    if (ctx.state < STATE_TOP)
    {
        if (ctx.state != new_state)
        {
            LOG(PRINT_INFO, "state %s", state_name[new_state]);
        }

        ctx.state = new_state;
    }
}

static void _reset_error(void)
{
    ctx.is_error_reset = false;
    ctx.motor_error_timer = 0;
    ctx.motor_find_overcurrent = false;
}

static void _state_init(void)
{
    _change_state(STATE_IDLE);
}

static void _state_idle(void)
{
    ctx.motor_find_overcurrent = false;
    if (menuGetValue(MENU_START_SYSTEM))
    {
        _change_state(STATE_WORKING);
    }
}

static void _state_working(void)
{
    if (menuGetValue(MENU_START_SYSTEM) == 0)
    {
        _change_state(STATE_IDLE);
    }

    /* Motor error overcurrent */
    uint32_t motor_current = menuGetValue(MENU_CURRENT_MOTOR);

    // LOG(PRINT_INFO, "Motor current %d", motor_current);
    if (motor_current > 40000)
    {
        if (!ctx.motor_find_overcurrent)
        {
            LOG(PRINT_INFO, "find motor overcurrent");
            ctx.motor_find_overcurrent = true;
            ctx.motor_error_timer = MS2ST(1500) + xTaskGetTickCount();
        }
        else
        {
            if (ctx.motor_error_timer < xTaskGetTickCount())
            {
                _change_state(STATE_ERROR_MOTOR_CURRENT);
            }
        }
    }
    else
    {
        if (ctx.motor_find_overcurrent)
        {
            LOG(PRINT_INFO, "reset motor overcurrent");
        }

        ctx.motor_find_overcurrent = false;
    }

    uint32_t check_measure = 0;
    /* Motor error not connected */
    check_measure = measure_get_filtered_value(MEAS_CH_CHECK_MOTOR);
    LOG(PRINT_INFO, "Motor %d", check_measure);
    if (!menuGetValue(MENU_MOTOR_IS_ON) && check_measure < 100 && srvrControllIsWorking())
    {
        if (!ctx.motor_find_not_connected)
        {
            LOG(PRINT_INFO, "find motor not connected");
            ctx.motor_find_not_connected = true;
            ctx.motor_not_connected_timer = MS2ST(500) + xTaskGetTickCount();
        }
        else
        {
            if (ctx.motor_not_connected_timer < xTaskGetTickCount())
            {
                _change_state(STATE_ERROR_MOTOR_NOT_CONNECTED);
            }
        }
    }
    else
    {
        if (ctx.motor_find_not_connected)
        {
            LOG(PRINT_INFO, "reset motor not connected");
        }

        ctx.motor_find_not_connected = false;
    }

    /* Vibro error not connected */
    check_measure = measure_get_filtered_value(MEAS_CH_CHECK_VIBRO);
    LOG(PRINT_INFO, "Vibro %d", check_measure);
    if (!vibro_is_on() && check_measure < 100 && srvrControllIsWorking())
    {
        if (!ctx.vibro_find_not_connected)
        {
            LOG(PRINT_INFO, "find vibro not connected");
            ctx.vibro_find_not_connected = true;
            ctx.vibro_not_connected_timer = MS2ST(500) + xTaskGetTickCount();
        }
        else
        {
            if (ctx.vibro_not_connected_timer < xTaskGetTickCount())
            {
                _change_state(STATE_ERROR_VIBRO_NOT_CONNECTED);
            }
        }
    }
    else
    {
        if (ctx.vibro_find_not_connected)
        {
            LOG(PRINT_INFO, "reset vibro not connected");
        }

        ctx.vibro_find_not_connected = false;
    }
}

static void _state_error_temperature(void)
{
}

static void _state_error_mototr_current(void)
{
    if (srvrConrollerSetError(ERROR_MOTOR_OVER_CURRENT))
    {
        _change_state(STATE_WAIT_RESET_ERROR);
    }
    else
    {
        _reset_error();
        _change_state(STATE_IDLE);
    }
}

static void _state_error_motor_not_connected(void)
{
    printf("%s", __func__);
    if (srvrConrollerSetError(ERROR_MOTOR_NOT_CONNECTED))
    {
        _change_state(STATE_WAIT_RESET_ERROR);
    }
    else
    {
        _reset_error();
        _change_state(STATE_IDLE);
    }
}

static void _state_error_vibro_not_connected(void)
{
    if (srvrConrollerSetError(ERROR_VIBRO_NOT_CONNECTED))
    {
        _change_state(STATE_WAIT_RESET_ERROR);
    }
    else
    {
        _reset_error();
        _change_state(STATE_IDLE);
    }
}

static void _state_error_vibro(void)
{
}

static void _state_wait_reset_error(void)
{
    if (ctx.is_error_reset)
    {
        _reset_error();
        _change_state(STATE_IDLE);
    }
}

static void _error_task(void *arg)
{
    //static uint32_t error_event_timer;
    while (1)
    {
        switch (ctx.state)
        {
        case STATE_INIT:
            _state_init();
            break;

        case STATE_IDLE:
            _state_idle();
            break;

        case STATE_WORKING:
            _state_working();
            break;

        case STATE_ERROR_TEMPERATURE:
            _state_error_temperature();
            break;

        case STATE_ERROR_MOTOR_CURRENT:
            _state_error_mototr_current();
            break;

        case STATE_ERROR_MOTOR_NOT_CONNECTED:
            _state_error_motor_not_connected();
            break;

        case STATE_ERROR_VIBRO_NOT_CONNECTED:
            _state_error_vibro_not_connected();
            break;

        case STATE_ERROR_VIBRO:
            _state_error_vibro();
            break;

        case STATE_WAIT_RESET_ERROR:
            _state_wait_reset_error();
            break;

        default:
            ctx.state = STATE_INIT;
            break;
        }

        vTaskDelay(200 / portTICK_RATE_MS);
    } //error_event_timer
}

void errorSolarkaStart(void)
{
    xTaskCreate(_error_task, "_error_task", 4096, NULL, NORMALPRIO, NULL);
}

void errorSolarkaErrorReset(void)
{
    printf("%s\n\r", __func__);
    ctx.is_error_reset = true;
}

#endif
