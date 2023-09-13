#include "config.h"
#include "menu.h"
#include "menu_drv.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "menu_default.h"
#include "menu_param.h"
#include "wifidrv.h"
#include "cmd_client.h"
#include "menu_backend.h"
#include "menu_settings.h"
#include "fast_add.h"
#include "led.h"

#define MODULE_NAME "[SETTING] "
#define DEBUG_LVL PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG(_lvl, ...) \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

typedef enum
{
    MENU_LIST_PARAMETERS,
    MENU_EDIT_PARAMETERS,
    MENU_TOP,
} menu_state_t;

typedef enum
{
    PARAM_BOOTUP_MENU,
    PARAM_BUZZER,
    PARAM_BRIGHTNESS,
    PARAM_LANGUAGE,
    PARAM_POWER_ON_MIN,
    PARAM_PERIOD,
    PARAM_MOTOR_ERROR,
    PARAM_SERVO_ERROR,
    PARAM_VIBRO_ERROR,
    PARAM_MOTOR_ERROR_CALIBRATION,
    PARAM_SERVO_CLOSE_CALIBRATION,
    PARAM_SERVO_OPEN_CALIBRATION,
    PARAM_SILOS_HEIGHT,
    PARAM_TOP,
} parameters_type_t;

typedef enum
{
    UNIT_INT,
    UNIT_ON_OFF,
    UNIT_LANGUAGE,
    UNIT_BOOL,
} unit_type_t;

typedef struct
{
    enum dictionary_phrase name_dict;
    parameters_type_t param_type;
    uint32_t value;
    uint32_t max_value;
    uint32_t min_value;
    const char *unit_name;
    unit_type_t unit_type;
    void (*get_value)(uint32_t *value);
    void (*get_max_value)(uint32_t *value);
    void (*get_min_value)(uint32_t *value);
    void (*set_value)(uint32_t value);
    void (*enter)(void);
    void (*exit)(void);
} parameters_t;

static void get_servo_close_calibration(uint32_t *value);
static void get_servo_open_calibration(uint32_t *value);
static void get_max_servo_close_calibration(uint32_t *value);
static void get_max_servo_open_calibration(uint32_t *value);
static void set_servo_close_calibration(uint32_t value);
static void set_servo_open_calibration(uint32_t value);
static void enter_servo_close_calibration(void);
static void exit_servo_close_calibration(void);
static void enter_servo_open_calibration(void);
static void exit_servo_open_calibration(void);

static void get_silos_height(uint32_t *value);
static void set_silos_height(uint32_t value);
static void get_max_silos_height(uint32_t *value);
static void get_min_silos_height(uint32_t *value);

static void get_bootup(uint32_t *value);
static void get_buzzer(uint32_t *value);
static void get_max_bootup(uint32_t *value);
static void get_max_buzzer(uint32_t *value);
static void set_bootup(uint32_t value);
static void set_buzzer(uint32_t value);

static void get_language(uint32_t *value);
static void get_max_language(uint32_t *value);
static void set_language(uint32_t value);

static void get_power_on_min(uint32_t *value);
static void get_max_power_on_min(uint32_t *value);
static void get_min_power_on_min(uint32_t *value);
static void set_power_on_min(uint32_t value);

static void get_motor_error(uint32_t *value);
static void get_max_motor_error(uint32_t *value);
static void set_motor_error(uint32_t value);
static void exit_motor_error(void);

static void get_servo_error(uint32_t *value);
static void get_max_servo_error(uint32_t *value);
static void set_servo_error(uint32_t value);
static void exit_servo_error(void);

static void get_period(uint32_t *value);
static void get_max_period(uint32_t *value);
static void get_min_period(uint32_t *value);
static void set_period(uint32_t value);
static void exit_period(void);

static void get_motor_error_calibration(uint32_t *value);
static void set_motor_error_calibration(uint32_t value);
static void get_max_motor_error_calibration(uint32_t *value);
static void exit_motor_error_calibration(void);

static void get_brightness(uint32_t *value);
static void set_brightness(uint32_t value);
static void get_max_brightness(uint32_t *value);
static void get_min_brightness(uint32_t *value);
static void enter_brightness(void);
static void exit_brightness(void);

static const char *language[] =
    {
        [MENU_LANGUAGE_ENGLISH] = "English",
        [MENU_LANGUAGE_RUSSIAN] = "Russian",
        [MENU_LANGUAGE_POLISH] = "Polski",
        [MENU_LANGUAGE_GERMANY] = "Germany",
};

static parameters_t *parameters_list;
static uint32_t parameters_size;

static parameters_t parameters_list_siewnik[] =
    {
        {.param_type = PARAM_BOOTUP_MENU,
         .name_dict = DICT_BOOTING,
         .unit_type = UNIT_ON_OFF,
         .get_value = get_bootup,
         .set_value = set_bootup,
         .get_max_value = get_max_bootup},

        {.param_type = PARAM_BRIGHTNESS,
         .name_dict = DICT_BRIGHTNESS,
         .unit_type = UNIT_INT,
         .get_value = get_brightness,
         .set_value = set_brightness,
         .get_max_value = get_max_brightness,
         .get_min_value = get_min_brightness,
         .enter = enter_brightness,
         .exit = exit_brightness},

        {.param_type = PARAM_BUZZER,
         .name_dict = DICT_BUZZER,
         .unit_type = UNIT_ON_OFF,
         .get_value = get_buzzer,
         .set_value = set_buzzer,
         .get_max_value = get_max_buzzer},

        {.param_type = PARAM_LANGUAGE,
         .name_dict = DICT_LANGUAGE,
         .unit_type = UNIT_LANGUAGE,
         .get_value = get_language,
         .set_value = set_language,
         .get_max_value = get_max_language},

        {.param_type = PARAM_POWER_ON_MIN,
         .name_dict = DICT_IDLE_TIME,
         .unit_type = UNIT_INT,
         .unit_name = "[min]",
         .get_value = get_power_on_min,
         .set_value = set_power_on_min,
         .get_max_value = get_max_power_on_min,
         .get_min_value = get_min_power_on_min},

        {.param_type = PARAM_MOTOR_ERROR,
         .name_dict = DICT_MOTOR_ERR,
         .unit_type = UNIT_ON_OFF,
         .get_value = get_motor_error,
         .set_value = set_motor_error,
         .get_max_value = get_max_motor_error,
         .exit = exit_motor_error},

        {.param_type = PARAM_SERVO_ERROR,
         .name_dict = DICT_SERVO_ERR,
         .unit_type = UNIT_ON_OFF,
         .get_value = get_servo_error,
         .set_value = set_servo_error,
         .get_max_value = get_max_servo_error,
         .exit = exit_servo_error},

        {.param_type = PARAM_MOTOR_ERROR_CALIBRATION,
         .name_dict = DICT_MOTOR_ERROR_CALIBRATION,
         .unit_type = UNIT_INT,
         .get_value = get_motor_error_calibration,
         .set_value = set_motor_error_calibration,
         .get_max_value = get_max_motor_error_calibration,
         .exit = exit_motor_error_calibration},

        {.param_type = PARAM_SERVO_CLOSE_CALIBRATION,
         .name_dict = DICT_SERVO_CLOSE,
         .unit_type = UNIT_INT,
         .get_value = get_servo_close_calibration,
         .set_value = set_servo_close_calibration,
         .get_max_value = get_max_servo_close_calibration,
         .enter = enter_servo_close_calibration,
         .exit = exit_servo_close_calibration},

        {.param_type = PARAM_SERVO_OPEN_CALIBRATION,
         .name_dict = DICT_SERVO_OPEN,
         .unit_type = UNIT_INT,
         .get_value = get_servo_open_calibration,
         .set_value = set_servo_open_calibration,
         .get_max_value = get_max_servo_open_calibration,
         .enter = enter_servo_open_calibration,
         .exit = exit_servo_open_calibration},

        {.param_type = PARAM_SILOS_HEIGHT,
         .name_dict = DICT_SILOS_HEIGHT,
         .unit_type = UNIT_INT,
         .get_value = get_silos_height,
         .set_value = set_silos_height,
         .get_max_value = get_max_silos_height,
         .get_min_value = get_min_silos_height,
         .unit_name = "[cm]"},
};

static parameters_t parameters_list_solarka[] =
    {

        {.param_type = PARAM_BOOTUP_MENU,
         .name_dict = DICT_BOOTING,
         .unit_type = UNIT_ON_OFF,
         .get_value = get_bootup,
         .set_value = set_bootup,
         .get_max_value = get_max_bootup},

        {.param_type = PARAM_BRIGHTNESS,
         .name_dict = DICT_BRIGHTNESS,
         .unit_type = UNIT_INT,
         .get_value = get_brightness,
         .set_value = set_brightness,
         .get_max_value = get_max_brightness,
         .get_min_value = get_min_brightness,
         .enter = enter_brightness,
         .exit = exit_brightness},

        {.param_type = PARAM_BUZZER,
         .name_dict = DICT_BUZZER,
         .unit_type = UNIT_ON_OFF,
         .get_value = get_buzzer,
         .set_value = set_buzzer,
         .get_max_value = get_max_buzzer},

        {.param_type = PARAM_LANGUAGE,
         .name_dict = DICT_LANGUAGE,
         .unit_type = UNIT_LANGUAGE,
         .get_value = get_language,
         .set_value = set_language,
         .get_max_value = get_max_language},
        {.param_type = PARAM_POWER_ON_MIN,
         .name_dict = DICT_IDLE_TIME,
         .unit_type = UNIT_INT,
         .unit_name = "[min]",
         .get_value = get_power_on_min,
         .set_value = set_power_on_min,
         .get_max_value = get_max_power_on_min,
         .get_min_value = get_min_power_on_min},

        {.param_type = PARAM_MOTOR_ERROR,
         .name_dict = DICT_MOTOR_ERR,
         .unit_type = UNIT_ON_OFF,
         .get_value = get_motor_error,
         .set_value = set_motor_error,
         .get_max_value = get_max_motor_error,
         .exit = exit_motor_error},

        {.param_type = PARAM_VIBRO_ERROR,
         .name_dict = DICT_VIBRO_ERR,
         .unit_type = UNIT_ON_OFF,
         .get_value = get_servo_error,
         .set_value = set_servo_error,
         .get_max_value = get_max_servo_error,
         .exit = exit_servo_error},

        {.param_type = PARAM_PERIOD,
         .name_dict = DICT_PERIOD,
         .unit_type = UNIT_INT,
         .unit_name = "[s]",
         .get_value = get_period,
         .set_value = set_period,
         .get_max_value = get_max_period,
         .exit = exit_period,
         .get_min_value = get_min_period},

        {.param_type = PARAM_MOTOR_ERROR_CALIBRATION,
         .name_dict = DICT_MOTOR_ERROR_CALIBRATION,
         .unit_type = UNIT_INT,
         .get_value = get_motor_error_calibration,
         .set_value = set_motor_error_calibration,
         .get_max_value = get_max_motor_error_calibration,
         .exit = exit_motor_error_calibration},
};

static scrollBar_t scrollBar =
    {
        .line_max = MAX_LINE,
        .y_start = MENU_HEIGHT};

static menu_state_t _state;

static void get_max_servo_close_calibration(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_CLOSE_SERVO_REGULATION);
}

static void get_max_servo_open_calibration(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_OPEN_SERVO_REGULATION);
}

static void get_servo_close_calibration(uint32_t *value)
{
    *value = menuGetValue(MENU_CLOSE_SERVO_REGULATION);
}

static void get_servo_open_calibration(uint32_t *value)
{
    *value = menuGetValue(MENU_OPEN_SERVO_REGULATION);
}

static void set_servo_close_calibration(uint32_t value)
{
    LOG(PRINT_DEBUG, "%s: %d", __func__, value);
    cmdClientSetValueWithoutResp(MENU_CLOSE_SERVO_REGULATION, value);
}

static void set_servo_open_calibration(uint32_t value)
{
    LOG(PRINT_DEBUG, "%s: %d", __func__, value);
    cmdClientSetValueWithoutResp(MENU_OPEN_SERVO_REGULATION, value);
}

static void get_silos_height(uint32_t *value)
{
    *value = menuGetValue(MENU_SILOS_HEIGHT);
}

static void set_silos_height(uint32_t value)
{
    LOG(PRINT_DEBUG, "%s: %d", __func__, value);
    cmdClientSetValueWithoutResp(MENU_SILOS_HEIGHT, value);
}

static void get_max_silos_height(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_SILOS_HEIGHT);
}

static void get_min_silos_height(uint32_t *value)
{
    *value = 10;
}

static void enter_servo_close_calibration(void)
{
    LOG(PRINT_DEBUG, "%s", __func__);
    cmdClientSetValueWithoutResp(MENU_CLOSE_SERVO_REGULATION_FLAG, 1);
}

static void exit_servo_close_calibration(void)
{
    LOG(PRINT_DEBUG, "%s", __func__);
    cmdClientSetValueWithoutResp(MENU_CLOSE_SERVO_REGULATION_FLAG, 0);
}

static void enter_servo_open_calibration(void)
{
    LOG(PRINT_DEBUG, "%s", __func__);
    cmdClientSetValueWithoutResp(MENU_OPEN_SERVO_REGULATION_FLAG, 1);
}

static void exit_servo_open_calibration(void)
{
    LOG(PRINT_DEBUG, "%s", __func__);
    cmdClientSetValueWithoutResp(MENU_OPEN_SERVO_REGULATION_FLAG, 0);
}

static void get_motor_error(uint32_t *value)
{
    *value = menuGetValue(MENU_ERROR_MOTOR);
}

static void get_max_motor_error(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_ERROR_MOTOR);
}

static void set_motor_error(uint32_t value)
{
    menuSetValue(MENU_ERROR_MOTOR, value);
}

static void exit_motor_error(void)
{
    LOG(PRINT_DEBUG, "%s", __func__);
    cmdClientSetValueWithoutResp(MENU_ERROR_MOTOR, menuGetValue(MENU_ERROR_MOTOR));
}

static void get_servo_error(uint32_t *value)
{
    *value = menuGetValue(MENU_ERROR_SERVO);
}

static void get_max_servo_error(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_ERROR_SERVO);
}

static void set_servo_error(uint32_t value)
{
    menuSetValue(MENU_ERROR_SERVO, value);
}

static void exit_servo_error(void)
{
    LOG(PRINT_DEBUG, "%s", __func__);
    cmdClientSetValueWithoutResp(MENU_ERROR_SERVO, menuGetValue(MENU_ERROR_SERVO));
}

static void get_period(uint32_t *value)
{
    *value = menuGetValue(MENU_PERIOD);
}

static void get_max_period(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_PERIOD);
}

static void get_min_period(uint32_t *value)
{
    // 5 sekund
    *value = 3;
}

static void set_period(uint32_t value)
{
    menuSetValue(MENU_PERIOD, value);
}

static void exit_period(void)
{
    LOG(PRINT_DEBUG, "%s", __func__);
    cmdClientSetValueWithoutResp(MENU_PERIOD, menuGetValue(MENU_PERIOD));
}

static void get_motor_error_calibration(uint32_t *value)
{
    *value = menuGetValue(MENU_ERROR_MOTOR_CALIBRATION);
}

static void set_motor_error_calibration(uint32_t value)
{
    menuSetValue(MENU_ERROR_MOTOR_CALIBRATION, value);
}

static void get_max_motor_error_calibration(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_ERROR_MOTOR_CALIBRATION);
}

static void exit_motor_error_calibration(void)
{
    LOG(PRINT_DEBUG, "%s", __func__);
    cmdClientSetValueWithoutResp(MENU_ERROR_MOTOR_CALIBRATION, menuGetValue(MENU_ERROR_MOTOR_CALIBRATION));
}

static void get_power_on_min(uint32_t *value)
{
    *value = menuGetValue(MENU_POWER_ON_MIN);
}

static void get_max_power_on_min(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_POWER_ON_MIN);
}

static void get_min_power_on_min(uint32_t *value)
{
    // 5 minut
    *value = 5;
}

static void set_power_on_min(uint32_t value)
{
    menuSetValue(MENU_POWER_ON_MIN, value);
}

static void get_language(uint32_t *value)
{
    *value = menuGetValue(MENU_LANGUAGE);
}

static void get_max_language(uint32_t *value)
{
    *value = MENU_LANGUAGE_TOP - 1;
}

static void set_language(uint32_t value)
{
    dictionary_set_language(value);
}

static void get_bootup(uint32_t *value)
{
    *value = menuGetValue(MENU_BOOTUP_SYSTEM);
}

static void get_buzzer(uint32_t *value)
{
    *value = menuGetValue(MENU_BUZZER);
}

static void get_max_bootup(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_BOOTUP_SYSTEM);
}

static void get_max_buzzer(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_BUZZER);
}

static void set_bootup(uint32_t value)
{
    menuSetValue(MENU_BOOTUP_SYSTEM, value);
}

static void set_buzzer(uint32_t value)
{
    menuSetValue(MENU_BUZZER, value);
}

static void get_brightness(uint32_t *value)
{
    *value = menuGetValue(MENU_BRIGHTNESS);
}

static void set_brightness(uint32_t value)
{
    menuSetValue(MENU_BRIGHTNESS, value);
    MOTOR_LED_SET_RED(1);
    SERVO_VIBRO_LED_SET_GREEN(1);
}

static void get_max_brightness(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_BRIGHTNESS);
}

static void get_min_brightness(uint32_t *value)
{
    *value = 1;
}

static void enter_brightness(void)
{
    MOTOR_LED_SET_RED(1);
    SERVO_VIBRO_LED_SET_GREEN(1);
}

static void exit_brightness(void)
{
    MOTOR_LED_SET_RED(0);
    SERVO_VIBRO_LED_SET_GREEN(0);
}

static void menu_button_up_callback(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    if (_state == MENU_EDIT_PARAMETERS)
    {
        if (parameters_list[menu->position].set_value != NULL)
        {
            parameters_list[menu->position].set_value(parameters_list[menu->position].value);
        }

        if (parameters_list[menu->position].exit != NULL)
        {
            parameters_list[menu->position].exit();
        }
    }

    menu->last_button = LAST_BUTTON_UP;
    if (menu->position > 0)
    {
        menu->position--;
    }

    if (_state == MENU_EDIT_PARAMETERS)
    {
        if (parameters_list[menu->position].get_value != NULL)
        {
            parameters_list[menu->position].get_value(&parameters_list[menu->position].value);
        }

        if (parameters_list[menu->position].get_max_value != NULL)
        {
            parameters_list[menu->position].get_max_value(&parameters_list[menu->position].max_value);
        }

        if (parameters_list[menu->position].get_min_value != NULL)
        {
            parameters_list[menu->position].get_min_value(&parameters_list[menu->position].min_value);
        }

        if (parameters_list[menu->position].enter != NULL)
        {
            parameters_list[menu->position].enter();
        }
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

    if (_state == MENU_EDIT_PARAMETERS)
    {
        if (parameters_list[menu->position].set_value != NULL)
        {
            parameters_list[menu->position].set_value(parameters_list[menu->position].value);
        }

        if (parameters_list[menu->position].exit != NULL)
        {
            parameters_list[menu->position].exit();
        }
    }

    menu->last_button = LAST_BUTTON_DOWN;

    if (menu->position < parameters_size - 1)
    {
        menu->position++;
    }

    if (_state == MENU_EDIT_PARAMETERS)
    {
        if (parameters_list[menu->position].get_value != NULL)
        {
            parameters_list[menu->position].get_value(&parameters_list[menu->position].value);
        }

        if (parameters_list[menu->position].get_max_value != NULL)
        {
            parameters_list[menu->position].get_max_value(&parameters_list[menu->position].max_value);
        }

        if (parameters_list[menu->position].get_min_value != NULL)
        {
            parameters_list[menu->position].get_min_value(&parameters_list[menu->position].min_value);
        }

        if (parameters_list[menu->position].enter != NULL)
        {
            parameters_list[menu->position].enter();
        }
    }
}

static void menu_button_plus_callback(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    if (menu->position >= parameters_size)
    {
        return;
    }

    if (parameters_list[menu->position].value < parameters_list[menu->position].max_value)
    {
        parameters_list[menu->position].value++;
        if (parameters_list[menu->position].set_value != NULL)
        {
            parameters_list[menu->position].set_value(parameters_list[menu->position].value);
        }
    }
}

static void _fast_add_cb(uint32_t value)
{
    debug_function_name(__func__);
    (void)value;
}

static void menu_button_plus_time_cb(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    if (menu->position >= parameters_size)
    {
        return;
    }

    fastProcessStart(&parameters_list[menu->position].value, parameters_list[menu->position].max_value, 0, FP_PLUS, _fast_add_cb);
}

static void menu_button_minus_time_cb(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    if (menu->position >= parameters_size)
    {
        return;
    }

    fastProcessStart(&parameters_list[menu->position].value, parameters_list[menu->position].max_value, parameters_list[menu->position].min_value, FP_MINUS, _fast_add_cb);
}

static void menu_button_m_p_pull_cb(void *arg)
{
    for (int i = 0; i < parameters_size; i++)
    {
        fastProcessStop(&parameters_list[i].value);
    }
}

static void menu_button_minus_callback(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    if (menu->position >= parameters_size)
    {
        return;
    }

    if (parameters_list[menu->position].value > parameters_list[menu->position].min_value)
    {
        parameters_list[menu->position].value--;
        if (parameters_list[menu->position].set_value != NULL)
        {
            parameters_list[menu->position].set_value(parameters_list[menu->position].value);
        }
    }
}

static void menu_button_enter_callback(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    if (menu->position >= parameters_size)
    {
        LOG(PRINT_INFO, "Error settings: menu->position >= parameters_size");
        return;
    }

    if (_state == MENU_EDIT_PARAMETERS)
    {
        _state = MENU_LIST_PARAMETERS;
        if (parameters_list[menu->position].set_value != NULL)
        {
            parameters_list[menu->position].set_value(parameters_list[menu->position].value);
        }

        if (parameters_list[menu->position].enter != NULL)
        {
            parameters_list[menu->position].exit();
        }
    }
    else
    {
        _state = MENU_EDIT_PARAMETERS;
        if (parameters_list[menu->position].get_value != NULL)
        {
            parameters_list[menu->position].get_value(&parameters_list[menu->position].value);
        }

        if (parameters_list[menu->position].get_max_value != NULL)
        {
            parameters_list[menu->position].get_max_value(&parameters_list[menu->position].max_value);
        }

        if (parameters_list[menu->position].get_min_value != NULL)
        {
            parameters_list[menu->position].get_min_value(&parameters_list[menu->position].min_value);
        }

        if (parameters_list[menu->position].enter != NULL)
        {
            parameters_list[menu->position].enter();
        }
    }
}

static void menu_button_exit_callback(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    menuSaveParameters();
    if (_state == MENU_EDIT_PARAMETERS)
    {
        if (parameters_list[menu->position].set_value != NULL)
        {
            parameters_list[menu->position].set_value(parameters_list[menu->position].value);
        }

        if (parameters_list[menu->position].exit != NULL)
        {
            parameters_list[menu->position].exit();
        }

        _state = MENU_LIST_PARAMETERS;
        return;
    }

    menuExit(menu);
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
    menu->button.motor_on.fall_callback = menu_button_enter_callback;
    menu->button.exit.fall_callback = menu_button_exit_callback;

    menu->button.up_minus.fall_callback = menu_button_minus_callback;
    menu->button.up_minus.timer_callback = menu_button_minus_time_cb;
    menu->button.up_minus.rise_callback = menu_button_m_p_pull_cb;

    menu->button.up_plus.fall_callback = menu_button_plus_callback;
    menu->button.up_plus.timer_callback = menu_button_plus_time_cb;
    menu->button.up_plus.rise_callback = menu_button_m_p_pull_cb;

    menu->button.down_minus.fall_callback = menu_button_minus_callback;
    menu->button.down_minus.timer_callback = menu_button_minus_time_cb;
    menu->button.down_minus.rise_callback = menu_button_m_p_pull_cb;

    menu->button.down_plus.fall_callback = menu_button_plus_callback;
    menu->button.down_plus.timer_callback = menu_button_plus_time_cb;
    menu->button.down_plus.rise_callback = menu_button_m_p_pull_cb;

    if (config.dev_type == T_DEV_TYPE_SIEWNIK)
    {
        parameters_list = parameters_list_siewnik;
        parameters_size = sizeof(parameters_list_siewnik) / sizeof(parameters_list_siewnik[0]);
    }
    else
    {
        parameters_list = parameters_list_solarka;
        parameters_size = sizeof(parameters_list_solarka) / sizeof(parameters_list_solarka[0]);
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

    _state = MENU_LIST_PARAMETERS;

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
    static char buff[64];
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return false;
    }

    oled_clearScreen();

    switch (_state)
    {
    case MENU_LIST_PARAMETERS:
    {
        oled_setGLCDFont(OLED_FONT_SIZE_16);
        oled_printFixed(2, 0, dictionary_get_string(menu->name_dict), OLED_FONT_SIZE_16);
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

            LOG(PRINT_INFO, "menu->line.start %d, menu->line.end %d, position %d, menu->last_button %d\n",
                menu->line.start, menu->line.end, menu->position, menu->last_button);
        }

        int line = 0;
        do
        {
            int pos = line + menu->line.start;
            sprintf(buff, "%s", dictionary_get_string(parameters_list[pos].name_dict));
            if (line + menu->line.start == menu->position)
            {
                ssdFigureFillLine(MENU_HEIGHT + LINE_HEIGHT * line, LINE_HEIGHT);
                oled_printFixedBlack(2, MENU_HEIGHT + LINE_HEIGHT * line, buff, OLED_FONT_SIZE_11);
            }
            else
            {
                oled_printFixed(2, MENU_HEIGHT + LINE_HEIGHT * line, buff, OLED_FONT_SIZE_11);
            }

            line++;
        } while (line + menu->line.start != parameters_size && line < MAX_LINE);

        scrollBar.actual_line = menu->position;
        scrollBar.all_line = parameters_size - 1;
        ssdFigureDrawScrollBar(&scrollBar);
    }
    break;

    case MENU_EDIT_PARAMETERS:
        oled_printFixed(2, 0, dictionary_get_string(parameters_list[menu->position].name_dict), OLED_FONT_SIZE_11);
        switch (parameters_list[menu->position].unit_type)
        {
        case UNIT_INT:
            sprintf(buff, "%d %s", parameters_list[menu->position].value, parameters_list[menu->position].unit_name != NULL ? parameters_list[menu->position].unit_name : "");
            oled_printFixed(30, MENU_HEIGHT + 15, buff, OLED_FONT_SIZE_16);
            break;

        case UNIT_ON_OFF:
            sprintf(buff, "%s", parameters_list[menu->position].value ? dictionary_get_string(DICT_ON) : dictionary_get_string(DICT_OFF));
            oled_printFixed(30, MENU_HEIGHT + 15, buff, OLED_FONT_SIZE_16);
            break;

        case UNIT_BOOL:
            sprintf(buff, "%s", parameters_list[menu->position].value ? "1" : "0");
            oled_printFixed(30, MENU_HEIGHT + 15, buff, OLED_FONT_SIZE_16);
            break;

        case UNIT_LANGUAGE:
            sprintf(buff, "%s", language[parameters_list[menu->position].value]);
            oled_printFixed(30, MENU_HEIGHT + 15, buff, OLED_FONT_SIZE_16);
            break;
        default:
            break;
        }

        break;

    default:
        _state = MENU_LIST_PARAMETERS;
        break;
    }

    return true;
}

void menuInitSettingsMenu(menu_token_t *menu)
{
    menu->menu_cb.enter = menu_enter_cb;
    menu->menu_cb.button_init_cb = menu_button_init_cb;
    menu->menu_cb.exit = menu_exit_cb;
    menu->menu_cb.process = menu_process;
}
