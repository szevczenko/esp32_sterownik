#include <stdint.h>
#include "measure.h"
#include "error_siewnik.h"
#include "motor.h"
#include "menu.h"
#include "servo.h"
#include "math.h"
#include "menu_param.h"

#include "cmd_server.h"
#include "server_controller.h"

#if CONFIG_DEVICE_SIEWNIK
#define MODULE_NAME    "[Err_siew] "
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
    STATE_ERROR_SERVO,
    STATE_WAIT_RESET_ERROR,
    STATE_TOP,
} state_t;

struct error_siewnik_ctx
{
    state_t state;
    TickType_t motor_error_timer;
    bool motor_find_overcurrent;

    TickType_t motor_not_connected_timer;
    bool motor_find_not_connected;

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
    [STATE_ERROR_SERVO] = "STATE_ERROR_SERVO",
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
    // if (motor_current > 15000)
    // {
    //  if (!ctx.motor_find_overcurrent)
    //  {
    //      LOG(PRINT_INFO, "find motor overcurrent");
    //      ctx.motor_find_overcurrent = true;
    //      ctx.motor_error_timer = MS2ST(5000) + xTaskGetTickCount();
    //  }
    //  else
    //  {
    //      if (ctx.motor_error_timer < xTaskGetTickCount())
    //      {
    //          _change_state(STATE_ERROR_MOTOR_CURRENT);
    //      }
    //  }
    // }
    // else
    // {
    //  if (ctx.motor_find_overcurrent)
    //  {
    //      LOG(PRINT_INFO, "reset motor overcurrent");
    //  }
    //  ctx.motor_find_overcurrent = false;
    // }

    // /* Motor error not connected */
    // if (menuGetValue(MENU_MOTOR_IS_ON) && menuGetValue(MENU_MOTOR) > 0 && motor_current < 100)
    // {
    //  if (!ctx.motor_find_not_connected)
    //  {
    //      LOG(PRINT_INFO, "find motor not connected");
    //      ctx.motor_find_not_connected = true;
    //      ctx.motor_not_connected_timer = MS2ST(250) + xTaskGetTickCount();
    //  }
    //  else
    //  {
    //      if (ctx.motor_not_connected_timer < xTaskGetTickCount())
    //      {
    //          _change_state(STATE_ERROR_MOTOR_NOT_CONNECTED);
    //      }
    //  }
    // }
    // else
    // {
    //  if (ctx.motor_find_not_connected)
    //  {
    //      LOG(PRINT_INFO, "reset motor not connected");
    //  }
    //  ctx.motor_find_not_connected = false;
    // }
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

static void _state_error_servo(void)
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

        case STATE_ERROR_SERVO:
            _state_error_servo();
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

#if 0
static void set_error_state(error_reason_ reason)
{
    if (reason == ERR_REASON_SERVO)
    {
        cmdServerSetValueWithoutResp(MENU_SERVO_ERROR_OVERCURRENT, 1);
    }
    else
    {
        cmdServerSetValueWithoutResp(MENU_MOTOR_ERROR_OVERCURRENT, 1);
        servo_error(1);
    }

    error_events = 1;
}

int get_calibration_value(uint8_t type)
{
    return 100;
}

#define REZYSTANCJA_WIRNIKA    3

static float count_motor_error_value(uint16_t x, float volt_accum)
{
    float volt_in_motor = volt_accum * x / 100;
    float volt_in_motor_nominal = 14.2 * x / 100;
    float temp = 0.011 * pow(x, 1.6281) + (volt_in_motor - volt_in_motor_nominal) / REZYSTANCJA_WIRNIKA;

#if DARK_MENU
    temp += (float)(dark_menu_get_value(MENU_ERROR_MOTOR_CALIBRATION) - 50) * x / 400;
#endif

    /* Jak chcesz dobrac parametry mozesz dla testu odkomentowac linijke nizej LOG(PRINT_INFO, )
        Funkcja zwraca prad maksymalny
        x                       - wartosc na wyswietlaczu PWM
        volt                    - napiecie akumulatora
        0.011*pow(x, 1.6281)    - zalezosc wedlug twoich pomiarow wyznaczona w excel
        volt_in_motor           - napiecie podawane na silnik obecnie przeskalowane wedlug PWM
        volt_in_motor_nominal   - napiecie przy ktorym wykonane pomiary (14,2 V) przeskalowane wedlug PWM
     */

    //LOG(PRINT_INFO, "CURRENT volt_in: %d, volt_nominal: %f, out_value: %f\n", volt_in_motor, volt_in_motor_nominal, temp);
    return temp;
}

static uint16_t count_motor_timeout_wait(uint16_t x)
{
    uint16_t timeout = 5000 - x * 30;

    LOG(PRINT_INFO, "count_motor_timeout_wait: %d", timeout);
    return timeout; //5000[ms] - pwm*30
}

static uint16_t count_motor_timeout_axelerate(uint16_t x)
{
    uint16_t timeout = 5000 - x * 30;

    LOG(PRINT_INFO, "count_motor_timeout_axelerate: %d", timeout);
    return timeout; //5000[ms] - pwm*30
}

static uint16_t count_servo_error_value(void)
{
#if DARK_MENU
    int ret = dark_menu_get_value(MENU_ERROR_SERVO_CALIBRATION);
    if (ret < 0)
    {
        return 0;
    }
    else
    {
        return ret;
    }
#else
    return 20;
#endif
}

#endif

void errorSiewnikStart(void)
{
    xTaskCreate(_error_task, "_error_task", 4096, NULL, NORMALPRIO, NULL);
}

void errorSiewnikErrorReset(void)
{
    ctx.is_error_reset = true;
}

#endif
