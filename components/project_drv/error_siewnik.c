#include <stdint.h>
#include "measure.h"
#include "error_siewnik.h"
#include "motor.h"

#include "servo.h"
#include "math.h"
#include "parameters.h"

#include "cmd_server.h"
#include "server_controller.h"

#if CONFIG_DEVICE_SIEWNIK
#define MODULE_NAME "[Err_siew] "
#define DEBUG_LVL PRINT_WARNING

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
    STATE_ERROR_SERVO_CURRENT,
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

    TickType_t temperature_error_timer;
    bool temperature_find_overcurrent;

    TickType_t servo_blocking_error_timer;
    TickType_t servo_error_reset_timer;
    TickType_t servo_overcurrent_timer;
    bool servo_find_overcurrent;
    uint8_t servo_try_counter;

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
        [STATE_ERROR_SERVO_CURRENT] = "STATE_ERROR_SERVO_CURRENT",
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
    ctx.servo_find_overcurrent = false;
    ctx.servo_blocking_error_timer = MS2ST(2000) + xTaskGetTickCount();
    ctx.servo_try_counter = 0;
}

static bool _is_overcurrent(float motor_current)
{
    float max_current = 0.1 * parameters_getValue(PARAM_MOTOR) + 2;
    float calibration = ((float)parameters_getValue(PARAM_ERROR_MOTOR_CALIBRATION) - 50.0) * (float)parameters_getValue(PARAM_MOTOR) / 100.0;
    float overcurrent = max_current + calibration;

    LOG(PRINT_DEBUG, "Motor current %.2f overcurrent %.2f calib_val %d calib %.2f", motor_current, overcurrent, parameters_getValue(PARAM_ERROR_MOTOR_CALIBRATION), calibration);

    return motor_current > overcurrent;
}

static bool _is_servo_overcurrent(float servo_voltage)
{
    if (ctx.servo_blocking_error_timer < xTaskGetTickCount())
    {
        float calibration = ((float)parameters_getValue(PARAM_ERROR_SERVO_CALIBRATION) - 50.0) * 10;
        float overvoltage_mv = 500;

        LOG(PRINT_INFO, "Servo current %.2f overcurrent %.2f calib_val %d calib %.2f", servo_voltage, overvoltage_mv, parameters_getValue(PARAM_ERROR_SERVO_CALIBRATION), calibration);

        return servo_voltage > overvoltage_mv;
    }
    return false;
}

static void _state_init(void)
{
    _change_state(STATE_IDLE);
}

static void _state_idle(void)
{
    ctx.motor_find_overcurrent = false;
    if (parameters_getValue(PARAM_START_SYSTEM))
    {
        _change_state(STATE_WORKING);
    }
}

static void _state_working(void)
{
    if (parameters_getValue(PARAM_START_SYSTEM) == 0)
    {
        _change_state(STATE_IDLE);
    }

    /* Motor error overcurrent */

    LOG(PRINT_DEBUG, "Error motor %d, servo %d", parameters_getValue(PARAM_ERROR_MOTOR), parameters_getValue(PARAM_ERROR_SERVO));

    float motor_current = (float)parameters_getValue(PARAM_CURRENT_MOTOR) / 100;

    if (motor_current > 45)
    {
        _change_state(STATE_ERROR_MOTOR_CURRENT);
    }

    if (_is_overcurrent(motor_current) && parameters_getValue(PARAM_ERROR_MOTOR))
    {
        if (!ctx.motor_find_overcurrent)
        {
            LOG(PRINT_INFO, "find motor overcurrent");
            ctx.motor_find_overcurrent = true;
            ctx.motor_error_timer = MS2ST(2500) + xTaskGetTickCount();
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

    float servo_voltage = (float)parameters_getValue(PARAM_VOLTAGE_SERVO);

    if (_is_servo_overcurrent(servo_voltage) && parameters_getValue(PARAM_ERROR_SERVO))
    {
        if (!ctx.servo_find_overcurrent)
        {
            LOG(PRINT_INFO, "find servo overcurrent");
            ctx.servo_find_overcurrent = true;
            ctx.servo_overcurrent_timer = xTaskGetTickCount() + MS2ST(1000);
        }
        else
        {
            if (ctx.servo_overcurrent_timer < xTaskGetTickCount())
            {
                if (ctx.servo_try_counter < 3)
                {
                    ctx.servo_blocking_error_timer = MS2ST(2000) + xTaskGetTickCount();
                    ctx.servo_error_reset_timer = xTaskGetTickCount() + MS2ST(20000);
                    ctx.servo_try_counter++;
                    /* Send to servo try information */
                    servo_enable_try();
                }
                else
                {
                    _change_state(STATE_ERROR_SERVO);
                    ctx.servo_try_counter = 0;
                }
            }
        }
    }
    else
    {
        if (ctx.servo_error_reset_timer < xTaskGetTickCount())
        {
            LOG(PRINT_INFO, "reset servo counter");
            ctx.servo_try_counter = 0;
        }

        if (ctx.servo_find_overcurrent)
        {
            LOG(PRINT_INFO, "reset servo overcurrent");
        }
        ctx.servo_find_overcurrent = false;
    }

    uint32_t temperature = parameters_getValue(PARAM_TEMPERATURE);
    LOG(PRINT_INFO, "Temperature %d", temperature);
    if (temperature > 90 && parameters_getValue(PARAM_ERROR_MOTOR))
    {
        if (!ctx.temperature_find_overcurrent)
        {
            LOG(PRINT_INFO, "find temperature");
            ctx.temperature_find_overcurrent = true;
            ctx.temperature_error_timer = MS2ST(1500) + xTaskGetTickCount();
        }
        else
        {
            if (ctx.temperature_error_timer < xTaskGetTickCount())
            {
                _change_state(STATE_ERROR_TEMPERATURE);
            }
        }
    }
    else
    {
        if (ctx.temperature_find_overcurrent)
        {
            LOG(PRINT_INFO, "reset temperature overcurrent");
        }

        ctx.temperature_find_overcurrent = false;
    }

    /* Motor error not connected */
    // check_measure = measure_get_filtered_value(MEAS_CH_CHECK_MOTOR);
    // LOG(PRINT_DEBUG, "Motor %d", check_measure);
    // if (!parameters_getValue(PARAM_MOTOR_IS_ON) && check_measure < 100 && srvrControllIsWorking() && parameters_getValue(PARAM_ERROR_MOTOR))
    // {
    //     if (!ctx.motor_find_not_connected)
    //     {
    //         LOG(PRINT_INFO, "find motor not connected");
    //         ctx.motor_find_not_connected = true;
    //         ctx.motor_not_connected_timer = MS2ST(1250) + xTaskGetTickCount();
    //     }
    //     else
    //     {
    //         if (ctx.motor_not_connected_timer < xTaskGetTickCount())
    //         {
    //             _change_state(STATE_ERROR_MOTOR_NOT_CONNECTED);
    //         }
    //     }
    // }
    // else
    // {
    //     if (ctx.motor_find_not_connected)
    //     {
    //         LOG(PRINT_INFO, "reset motor not connected");
    //     }

    //     ctx.motor_find_not_connected = false;
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
    if (srvrConrollerSetError(ERROR_SERVO_OVER_CURRENT))
    {
        _change_state(STATE_WAIT_RESET_ERROR);
    }
    else
    {
        _reset_error();
        _change_state(STATE_IDLE);
    }
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
    // static uint32_t error_event_timer;
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

        vTaskDelay(MS2ST(200));
    } // error_event_timer
}

void errorSiewnikStart(void)
{
    xTaskCreate(_error_task, "_error_task", 4096, NULL, NORMALPRIO, NULL);
}

void errorSiewnikErrorReset(void)
{
    ctx.is_error_reset = true;
}

void errorSiewnikServoChangeState(void)
{
    ctx.servo_blocking_error_timer = MS2ST(2000) + xTaskGetTickCount();
    ctx.servo_error_reset_timer = xTaskGetTickCount() + MS2ST(20000);
    ctx.servo_try_counter = 0;
}

#endif
