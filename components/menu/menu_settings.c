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

#define MODULE_NAME    "[SETTING] "
#define DEBUG_LVL      PRINT_INFO

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
    MENU_LANGUAGE_ENGLISH,
    MENU_LANGUAGE_POLISH,
    MENU_LANGUAGE_GERMANY,
    MENU_LANGUAGE_TOP
} menu_language_t;

typedef enum
{
    PARAM_BOOTUP_MENU,
    PARAM_BUZZER,
    PARAM_LANGUAGE,
    PARAM_POWER_ON_MIN,
    PARAM_MOTOR_ERROR,
    PARAM_VIBRO_ERROR,
    #if CONFIG_DEVICE_SIEWNIK
    PARAM_SERVO_CLOSE_CALIBRATION,
    PARAM_SERVO_OPEN_CALIBRATION,
    #endif
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
    char *name;
    uint32_t value;
    uint32_t max_value;
    unit_type_t unit_type;
    void (*get_value)(uint32_t *value);
    void (*get_max_value)(uint32_t *value);
    void (*set_value)(uint32_t value);
    void (*enter)(void);
    void (*exit)(void);
} parameters_t;

#if CONFIG_DEVICE_SIEWNIK
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
#endif

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
static void set_power_on_min(uint32_t value);
static void get_motor_error(uint32_t *value);
static void get_max_motor_error(uint32_t *value);
static void set_motor_error(uint32_t value);
static void get_servo_error(uint32_t *value);
static void get_max_servo_error(uint32_t *value);
static void set_servo_error(uint32_t value);

static const char * language[] =
{
    [MENU_LANGUAGE_ENGLISH] = "English",
    [MENU_LANGUAGE_POLISH] = "Polish",
    [MENU_LANGUAGE_GERMANY] = "Germany",
};

static parameters_t parameters_list[] =
{
    [PARAM_BOOTUP_MENU] =
                                      {.name                                                                     =
                                          "Booting",
                                      .unit_type
                                          = UNIT_ON_OFF,
                                      .get_value                                                                 =
                                          get_bootup,
                                      .set_value                                                                 =
                                          set_bootup,
                                      .get_max_value                                                             =
                                          get_max_bootup              },
    [PARAM_BUZZER] =
                                      {.name                                                                     =
                                          "Buzzer",
                                      .unit_type
                                          = UNIT_ON_OFF,
                                      .get_value                                                                 =
                                          get_buzzer,
                                      .set_value                                                                 =
                                          set_buzzer,
                                      .get_max_value                                                             =
                                          get_max_buzzer              },
    [PARAM_LANGUAGE] = 
                                      {.name                                                                     =
                                          "Language",
                                      .unit_type
                                          = UNIT_LANGUAGE,
                                      .get_value                                                                 =
                                          get_language,
                                      .set_value                                                                 =
                                          set_language,
                                      .get_max_value                                                             =
                                          get_max_language              },
    [PARAM_POWER_ON_MIN] = 
                                      {.name                                                                     =
                                          "Idle time",
                                      .unit_type
                                          = UNIT_INT,
                                      .get_value                                                                 =
                                          get_power_on_min,
                                      .set_value                                                                 =
                                          set_power_on_min,
                                      .get_max_value                                                             =
                                          get_max_power_on_min              },

    [PARAM_MOTOR_ERROR] = 
                                      {.name                                                                     =
                                          "Motor err",
                                      .unit_type
                                          = UNIT_ON_OFF,
                                      .get_value                                                                 =
                                          get_motor_error,
                                      .set_value                                                                 =
                                          set_motor_error,
                                      .get_max_value                                                             =
                                          get_max_motor_error              },

    [PARAM_VIBRO_ERROR] = 
                                      {.name                                                                     =
                                          "Vibro err",
                                      .unit_type
                                          = UNIT_ON_OFF,
                                      .get_value                                                                 =
                                          get_servo_error,
                                      .set_value                                                                 =
                                          set_servo_error,
                                      .get_max_value                                                             =
                                          get_max_servo_error              },
#if CONFIG_DEVICE_SIEWNIK
    [PARAM_SERVO_CLOSE_CALIBRATION] =
                                      {.name                                                                     =
                                          "Servo close",
                                      .unit_type
                                          = UNIT_INT,
                                      .get_value                                                                 =
                                          get_servo_close_calibration,
                                      .set_value                                                                 =
                                          set_servo_close_calibration,
                                      .get_max_value                                                             =
                                          get_max_servo_close_calibration,
                                      .enter                                                                     =
                                          enter_servo_close_calibration,
                                      .exit
                                          = exit_servo_close_calibration},
    [PARAM_SERVO_OPEN_CALIBRATION] =
                                      {.name                                                                     =
                                          "Servo open",
                                      .unit_type
                                          = UNIT_INT,
                                      .get_value
                                          = get_servo_open_calibration,
                                      .set_value
                                          = set_servo_open_calibration,
                                      .get_max_value
                                          = get_max_servo_open_calibration,
                                      .enter
                                          = enter_servo_open_calibration,
                                      .exit
                                          = exit_servo_open_calibration},
#endif
};

static scrollBar_t scrollBar =
{
    .line_max = MAX_LINE,
    .y_start  = MENU_HEIGHT
};

static menu_state_t _state;

#if CONFIG_DEVICE_SIEWNIK
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
#endif

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

static void get_power_on_min(uint32_t *value)
{
    *value = menuGetValue(MENU_POWER_ON_MIN);
}

static void get_max_power_on_min(uint32_t *value)
{
    *value = menuGetMaxValue(MENU_POWER_ON_MIN);
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
    menuSetValue(MENU_LANGUAGE, value);
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
    if (menu->position < PARAM_TOP - 1)
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

    if (menu->position >= PARAM_TOP)
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

static void menu_button_minus_callback(void *arg)
{
    menu_token_t *menu = arg;

    if (menu == NULL)
    {
        NULL_ERROR_MSG();
        return;
    }

    if (menu->position >= PARAM_TOP)
    {
        return;
    }

    if (parameters_list[menu->position].value > 0)
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

    if (menu->position >= PARAM_TOP)
    {
        LOG(PRINT_INFO, "Error settings: menu->position >= PARAM_TOP");
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
    menu->button.exit.fall_callback = menu_button_exit_callback;
    menu->button.up_minus.fall_callback = menu_button_minus_callback;
    menu->button.up_plus.fall_callback = menu_button_plus_callback;
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
            oled_printFixed(2, 0, menu->name, STYLE_NORMAL);
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
               oled_setCursor(2, MENU_HEIGHT + LINE_HEIGHT * line);
               int pos = line + menu->line.start;
               sprintf(buff, "%s", parameters_list[pos].name);
               if (line + menu->line.start == menu->position)
               {
                   ssdFigureFillLine(MENU_HEIGHT + LINE_HEIGHT * line, LINE_HEIGHT);
                   oled_printBlack(buff);
               }
               else
               {
                   oled_print(buff);
               }

               line++;
           } while (line + menu->line.start != PARAM_TOP && line < MAX_LINE);

           scrollBar.actual_line = menu->position;
           scrollBar.all_line = PARAM_TOP - 1;
           ssdFigureDrawScrollBar(&scrollBar);
       }
       break;

    case MENU_EDIT_PARAMETERS:
        oled_setCursor(2, 0);
        oled_print(parameters_list[menu->position].name); //11x18 White
        switch (parameters_list[menu->position].unit_type)
        {
        case UNIT_INT:
            oled_setCursor(30, MENU_HEIGHT + LINE_HEIGHT * 2);
            sprintf(buff, "%d", parameters_list[menu->position].value);
            oled_print(buff);
            break;

        case UNIT_ON_OFF:
            oled_setCursor(30, MENU_HEIGHT + LINE_HEIGHT * 2);
            sprintf(buff, "%s", parameters_list[menu->position].value ? "ON" : "OFF");
            oled_print(buff);
            break;

        case UNIT_BOOL:
            oled_setCursor(30, MENU_HEIGHT + LINE_HEIGHT * 2);
            sprintf(buff, "%s", parameters_list[menu->position].value ? "1" : "0");
            oled_print(buff);
            break;

        case UNIT_LANGUAGE:
            oled_setCursor(30, MENU_HEIGHT + LINE_HEIGHT * 2);
            sprintf(buff, "%s", language[parameters_list[menu->position].value]);
            oled_print(buff);
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
