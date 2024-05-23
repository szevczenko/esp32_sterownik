#include "menu_settings.h"

#include "app_config.h"
#include "cmd_client.h"
#include "dictionary.h"
#include "fast_add.h"
#include "http_parameters_client.h"
#include "led.h"
#include "menu_backend.h"
#include "menu_default.h"
#include "menu_drv.h"
#include "parameters.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "wifi_menu.h"
#include "wifidrv.h"

#define MODULE_NAME "[SETTING] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

typedef enum
{
  MENU_LIST_PARAMETERS,
  MENU_EDIT_PARAMETERS,
  MENU_TOP,
} menu_state_t;

typedef enum
{
  SETTINGS_SN,
  SETTINGS_BOOTUP_MENU,
  SETTINGS_BUZZER,
  SETTINGS_BRIGHTNESS,
  SETTINGS_LANGUAGE,
  SETTINGS_POWER_ON_MIN,
  SETTINGS_PERIOD,
  SETTINGS_MOTOR_ERROR,
  SETTINGS_SERVO_ERROR,
  SETTINGS_VIBRO_ERROR,
  SETTINGS_MOTOR_ERROR_CALIBRATION,
  SETTINGS_SERVO_CLOSE_CALIBRATION,
  SETTINGS_SERVO_OPEN_CALIBRATION,
  SETTINGS_SILOS_HEIGHT,
  SETTINGS_VIBRO_PWM_DUTY,
  SETTINGS_TOP,
} parameters_type_t;

typedef enum
{
  UNIT_INT,
  UNIT_ON_OFF,
  UNIT_LANGUAGE,
  UNIT_BOOL,
  UNIT_STR,
} unit_type_t;

typedef struct
{
  enum dictionary_phrase name_dict;
  parameters_type_t param_type;
  uint32_t value;
  const char* str_value;
  uint32_t max_value;
  uint32_t min_value;
  const char* unit_name;
  unit_type_t unit_type;
  void ( *get_value )( uint32_t* value );
  void ( *get_max_value )( uint32_t* value );
  void ( *get_min_value )( uint32_t* value );
  void ( *set_value )( uint32_t value );
  const char* ( *get_str_value )( void );
  void ( *enter )( void );
  void ( *exit )( void );
  void ( *fast_add )( uint32_t value );
} parameters_t;

static void get_servo_close_calibration( uint32_t* value );
static void get_servo_open_calibration( uint32_t* value );
static void get_max_servo_close_calibration( uint32_t* value );
static void get_max_servo_open_calibration( uint32_t* value );
static void set_servo_close_calibration( uint32_t value );
static void set_servo_open_calibration( uint32_t value );
static void enter_servo_close_calibration( void );
static void exit_servo_close_calibration( void );
static void enter_servo_open_calibration( void );
static void exit_servo_open_calibration( void );
static void fast_add_open_servo_cb( uint32_t value );
static void fast_add_close_servo_cb( uint32_t value );

static void get_silos_height( uint32_t* value );
static void set_silos_height( uint32_t value );
static void get_max_silos_height( uint32_t* value );
static void get_min_silos_height( uint32_t* value );

static void get_bootup( uint32_t* value );
static void get_buzzer( uint32_t* value );
static void get_max_bootup( uint32_t* value );
static void get_max_buzzer( uint32_t* value );
static void set_bootup( uint32_t value );
static void set_buzzer( uint32_t value );

static void get_language( uint32_t* value );
static void get_max_language( uint32_t* value );
static void set_language( uint32_t value );

static void get_power_on_min( uint32_t* value );
static void get_max_power_on_min( uint32_t* value );
static void get_min_power_on_min( uint32_t* value );
static void set_power_on_min( uint32_t value );

static void get_motor_error( uint32_t* value );
static void get_max_motor_error( uint32_t* value );
static void set_motor_error( uint32_t value );
static void exit_motor_error( void );

static void get_servo_error( uint32_t* value );
static void get_max_servo_error( uint32_t* value );
static void set_servo_error( uint32_t value );
static void exit_servo_error( void );

static void get_period( uint32_t* value );
static void get_max_period( uint32_t* value );
static void get_min_period( uint32_t* value );
static void set_period( uint32_t value );
static void exit_period( void );

static void get_motor_error_calibration( uint32_t* value );
static void set_motor_error_calibration( uint32_t value );
static void get_max_motor_error_calibration( uint32_t* value );
static void exit_motor_error_calibration( void );

static void get_brightness( uint32_t* value );
static void set_brightness( uint32_t value );
static void get_max_brightness( uint32_t* value );
static void get_min_brightness( uint32_t* value );
static void enter_brightness( void );
static void exit_brightness( void );

static void get_vibro_duty_pwm( uint32_t* value );
static void set_vibro_duty_pwm( uint32_t value );
static void get_max_vibro_duty_pwm( uint32_t* value );
static void get_min_vibro_duty_pwm( uint32_t* value );

static const char* get_serial_number( void );

static const char* language[] =
  {
    [MENU_LANGUAGE_ENGLISH] = "English",
    [MENU_LANGUAGE_RUSSIAN] = "Russian",
    [MENU_LANGUAGE_POLISH] = "Polski",
    [MENU_LANGUAGE_GERMANY] = "Deutsch",
};

static parameters_t* parameters_list;
static uint32_t parameters_size;

static parameters_t parameters_list_siewnik[] =
  {
    { .param_type = SETTINGS_SN,
     .name_dict = DICT_SERIAL_NUMBER,
     .unit_type = UNIT_STR,
     .get_str_value = get_serial_number },

    { .param_type = SETTINGS_BOOTUP_MENU,
     .name_dict = DICT_BOOTING,
     .unit_type = UNIT_ON_OFF,
     .get_value = get_bootup,
     .set_value = set_bootup,
     .get_max_value = get_max_bootup },

    { .param_type = SETTINGS_BRIGHTNESS,
     .name_dict = DICT_BRIGHTNESS,
     .unit_type = UNIT_INT,
     .get_value = get_brightness,
     .set_value = set_brightness,
     .get_max_value = get_max_brightness,
     .get_min_value = get_min_brightness,
     .enter = enter_brightness,
     .exit = exit_brightness },

    { .param_type = SETTINGS_BUZZER,
     .name_dict = DICT_BUZZER,
     .unit_type = UNIT_ON_OFF,
     .get_value = get_buzzer,
     .set_value = set_buzzer,
     .get_max_value = get_max_buzzer },

    { .param_type = SETTINGS_LANGUAGE,
     .name_dict = DICT_LANGUAGE,
     .unit_type = UNIT_LANGUAGE,
     .get_value = get_language,
     .set_value = set_language,
     .get_max_value = get_max_language },

    { .param_type = SETTINGS_POWER_ON_MIN,
     .name_dict = DICT_IDLE_TIME,
     .unit_type = UNIT_INT,
     .unit_name = "[min]",
     .get_value = get_power_on_min,
     .set_value = set_power_on_min,
     .get_max_value = get_max_power_on_min,
     .get_min_value = get_min_power_on_min },

    { .param_type = SETTINGS_MOTOR_ERROR,
     .name_dict = DICT_MOTOR_ERR,
     .unit_type = UNIT_ON_OFF,
     .get_value = get_motor_error,
     .set_value = set_motor_error,
     .get_max_value = get_max_motor_error,
     .exit = exit_motor_error },

    { .param_type = SETTINGS_SERVO_ERROR,
     .name_dict = DICT_SERVO_ERR,
     .unit_type = UNIT_ON_OFF,
     .get_value = get_servo_error,
     .set_value = set_servo_error,
     .get_max_value = get_max_servo_error,
     .exit = exit_servo_error },

    { .param_type = SETTINGS_MOTOR_ERROR_CALIBRATION,
     .name_dict = DICT_MOTOR_ERROR_CALIBRATION,
     .unit_type = UNIT_INT,
     .get_value = get_motor_error_calibration,
     .set_value = set_motor_error_calibration,
     .get_max_value = get_max_motor_error_calibration,
     .exit = exit_motor_error_calibration },

    { .param_type = SETTINGS_SERVO_CLOSE_CALIBRATION,
     .name_dict = DICT_SERVO_CLOSE,
     .unit_type = UNIT_INT,
     .get_value = get_servo_close_calibration,
     .set_value = set_servo_close_calibration,
     .get_max_value = get_max_servo_close_calibration,
     .enter = enter_servo_close_calibration,
     .exit = exit_servo_close_calibration,
     .fast_add = fast_add_close_servo_cb },

    { .param_type = SETTINGS_SERVO_OPEN_CALIBRATION,
     .name_dict = DICT_SERVO_OPEN,
     .unit_type = UNIT_INT,
     .get_value = get_servo_open_calibration,
     .set_value = set_servo_open_calibration,
     .get_max_value = get_max_servo_open_calibration,
     .enter = enter_servo_open_calibration,
     .exit = exit_servo_open_calibration,
     .fast_add = fast_add_open_servo_cb },

    { .param_type = SETTINGS_SILOS_HEIGHT,
     .name_dict = DICT_SILOS_HEIGHT,
     .unit_type = UNIT_INT,
     .get_value = get_silos_height,
     .set_value = set_silos_height,
     .get_max_value = get_max_silos_height,
     .get_min_value = get_min_silos_height,
     .unit_name = "[cm]" },
};

static parameters_t parameters_list_solarka[] =
  {
    { .param_type = SETTINGS_SN,
     .name_dict = DICT_SERIAL_NUMBER,
     .unit_type = UNIT_STR,
     .get_str_value = get_serial_number },

    { .param_type = SETTINGS_BOOTUP_MENU,
     .name_dict = DICT_BOOTING,
     .unit_type = UNIT_ON_OFF,
     .get_value = get_bootup,
     .set_value = set_bootup,
     .get_max_value = get_max_bootup },

    { .param_type = SETTINGS_BRIGHTNESS,
     .name_dict = DICT_BRIGHTNESS,
     .unit_type = UNIT_INT,
     .get_value = get_brightness,
     .set_value = set_brightness,
     .get_max_value = get_max_brightness,
     .get_min_value = get_min_brightness,
     .enter = enter_brightness,
     .exit = exit_brightness },

    { .param_type = SETTINGS_BUZZER,
     .name_dict = DICT_BUZZER,
     .unit_type = UNIT_ON_OFF,
     .get_value = get_buzzer,
     .set_value = set_buzzer,
     .get_max_value = get_max_buzzer },

    { .param_type = SETTINGS_LANGUAGE,
     .name_dict = DICT_LANGUAGE,
     .unit_type = UNIT_LANGUAGE,
     .get_value = get_language,
     .set_value = set_language,
     .get_max_value = get_max_language },
    { .param_type = SETTINGS_POWER_ON_MIN,
     .name_dict = DICT_IDLE_TIME,
     .unit_type = UNIT_INT,
     .unit_name = "[min]",
     .get_value = get_power_on_min,
     .set_value = set_power_on_min,
     .get_max_value = get_max_power_on_min,
     .get_min_value = get_min_power_on_min },

    { .param_type = SETTINGS_MOTOR_ERROR,
     .name_dict = DICT_MOTOR_ERR,
     .unit_type = UNIT_ON_OFF,
     .get_value = get_motor_error,
     .set_value = set_motor_error,
     .get_max_value = get_max_motor_error,
     .exit = exit_motor_error },

    { .param_type = SETTINGS_VIBRO_ERROR,
     .name_dict = DICT_VIBRO_ERR,
     .unit_type = UNIT_ON_OFF,
     .get_value = get_servo_error,
     .set_value = set_servo_error,
     .get_max_value = get_max_servo_error,
     .exit = exit_servo_error },

    { .param_type = SETTINGS_VIBRO_PWM_DUTY,
     .name_dict = DICT_VIBRO_PWM_DUTY,
     .unit_type = UNIT_INT,
     .get_value = get_vibro_duty_pwm,
     .set_value = set_vibro_duty_pwm,
     .get_max_value = get_max_vibro_duty_pwm,
     .get_min_value = get_min_vibro_duty_pwm,
     .unit_name = "[%]" },

    { .param_type = SETTINGS_PERIOD,
     .name_dict = DICT_PERIOD,
     .unit_type = UNIT_INT,
     .unit_name = "[s]",
     .get_value = get_period,
     .set_value = set_period,
     .get_max_value = get_max_period,
     .exit = exit_period,
     .get_min_value = get_min_period },

    { .param_type = SETTINGS_MOTOR_ERROR_CALIBRATION,
     .name_dict = DICT_MOTOR_ERROR_CALIBRATION,
     .unit_type = UNIT_INT,
     .get_value = get_motor_error_calibration,
     .set_value = set_motor_error_calibration,
     .get_max_value = get_max_motor_error_calibration,
     .exit = exit_motor_error_calibration },

    { .param_type = SETTINGS_SILOS_HEIGHT,
     .name_dict = DICT_SILOS_HEIGHT,
     .unit_type = UNIT_INT,
     .get_value = get_silos_height,
     .set_value = set_silos_height,
     .get_max_value = get_max_silos_height,
     .get_min_value = get_min_silos_height,
     .unit_name = "[cm]" },
};

static scrollBar_t scrollBar =
  {
    .line_max = MAX_LINE,
    .y_start = MENU_HEIGHT };

static menu_state_t _state;

static void get_max_servo_close_calibration( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_CLOSE_SERVO_REGULATION );
}

static void get_max_servo_open_calibration( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_OPEN_SERVO_REGULATION );
}

static void get_servo_close_calibration( uint32_t* value )
{
  *value = parameters_getValue( PARAM_CLOSE_SERVO_REGULATION );
}

static void get_servo_open_calibration( uint32_t* value )
{
  *value = parameters_getValue( PARAM_OPEN_SERVO_REGULATION );
}

static void set_servo_close_calibration( uint32_t value )
{
  LOG( PRINT_DEBUG, "%s: %d", __func__, value );
  HTTPParamClient_SetU32ValueDontWait( PARAM_CLOSE_SERVO_REGULATION, value );
}

static void set_servo_open_calibration( uint32_t value )
{
  LOG( PRINT_DEBUG, "%s: %d", __func__, value );
  HTTPParamClient_SetU32ValueDontWait( PARAM_OPEN_SERVO_REGULATION, value );
}

static void fast_add_open_servo_cb( uint32_t value )
{
  LOG( PRINT_DEBUG, "%s: %d", __func__, value );
  HTTPParamClient_SetU32ValueDontWait( PARAM_OPEN_SERVO_REGULATION, value );
}

static void fast_add_close_servo_cb( uint32_t value )
{
  LOG( PRINT_DEBUG, "%s: %d", __func__, value );
  HTTPParamClient_SetU32ValueDontWait( PARAM_CLOSE_SERVO_REGULATION, value );
}

static void get_silos_height( uint32_t* value )
{
  *value = parameters_getValue( PARAM_SILOS_HEIGHT );
}

static void set_silos_height( uint32_t value )
{
  LOG( PRINT_DEBUG, "%s: %d", __func__, value );
  HTTPParamClient_SetU32ValueDontWait( PARAM_SILOS_HEIGHT, value );
}

static void get_max_silos_height( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_SILOS_HEIGHT );
}

static void get_min_silos_height( uint32_t* value )
{
  *value = 10;
}

static void enter_servo_close_calibration( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  HTTPParamClient_SetU32ValueDontWait( PARAM_CLOSE_SERVO_REGULATION_FLAG, 1 );
}

static void exit_servo_close_calibration( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  HTTPParamClient_SetU32ValueDontWait( PARAM_CLOSE_SERVO_REGULATION_FLAG, 0 );
}

static void enter_servo_open_calibration( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  HTTPParamClient_SetU32ValueDontWait( PARAM_OPEN_SERVO_REGULATION_FLAG, 1 );
}

static void exit_servo_open_calibration( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  HTTPParamClient_SetU32ValueDontWait( PARAM_OPEN_SERVO_REGULATION_FLAG, 0 );
}

static void get_motor_error( uint32_t* value )
{
  *value = parameters_getValue( PARAM_ERROR_MOTOR );
}

static void get_max_motor_error( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_ERROR_MOTOR );
}

static void set_motor_error( uint32_t value )
{
  parameters_setValue( PARAM_ERROR_MOTOR, value );
}

static void exit_motor_error( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR, parameters_getValue( PARAM_ERROR_MOTOR ) );
}

static void get_servo_error( uint32_t* value )
{
  *value = parameters_getValue( PARAM_ERROR_SERVO );
}

static void get_max_servo_error( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_ERROR_SERVO );
}

static void set_servo_error( uint32_t value )
{
  parameters_setValue( PARAM_ERROR_SERVO, value );
}

static void exit_servo_error( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_SERVO, parameters_getValue( PARAM_ERROR_SERVO ) );
}

static void get_period( uint32_t* value )
{
  *value = parameters_getValue( PARAM_PERIOD );
}

static void get_max_period( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_PERIOD );
}

static void get_min_period( uint32_t* value )
{
  // 5 sekund
  *value = 3;
}

static void set_period( uint32_t value )
{
  parameters_setValue( PARAM_PERIOD, value );
}

static void exit_period( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  HTTPParamClient_SetU32ValueDontWait( PARAM_PERIOD, parameters_getValue( PARAM_PERIOD ) );
}

static void get_motor_error_calibration( uint32_t* value )
{
  *value = parameters_getValue( PARAM_ERROR_MOTOR_CALIBRATION );
}

static void set_motor_error_calibration( uint32_t value )
{
  parameters_setValue( PARAM_ERROR_MOTOR_CALIBRATION, value );
}

static void get_max_motor_error_calibration( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_ERROR_MOTOR_CALIBRATION );
}

static void exit_motor_error_calibration( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR_CALIBRATION, parameters_getValue( PARAM_ERROR_MOTOR_CALIBRATION ) );
}

static void get_power_on_min( uint32_t* value )
{
  *value = parameters_getValue( PARAM_POWER_ON_MIN );
}

static void get_max_power_on_min( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_POWER_ON_MIN );
}

static void get_min_power_on_min( uint32_t* value )
{
  // 5 minut
  *value = 5;
}

static void set_power_on_min( uint32_t value )
{
  parameters_setValue( PARAM_POWER_ON_MIN, value );
}

static void get_language( uint32_t* value )
{
  *value = parameters_getValue( PARAM_LANGUAGE );
}

static void get_max_language( uint32_t* value )
{
  *value = MENU_LANGUAGE_TOP - 1;
}

static void set_language( uint32_t value )
{
  dictionary_set_language( value );
}

static void get_bootup( uint32_t* value )
{
  *value = parameters_getValue( PARAM_BOOT_UP_SYSTEM );
}

static void get_buzzer( uint32_t* value )
{
  *value = parameters_getValue( PARAM_BUZZER );
}

static void get_max_bootup( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_BOOT_UP_SYSTEM );
}

static void get_max_buzzer( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_BUZZER );
}

static void set_bootup( uint32_t value )
{
  parameters_setValue( PARAM_BOOT_UP_SYSTEM, value );
}

static void set_buzzer( uint32_t value )
{
  parameters_setValue( PARAM_BUZZER, value );
}

static void get_brightness( uint32_t* value )
{
  *value = parameters_getValue( PARAM_BRIGHTNESS );
}

static void set_brightness( uint32_t value )
{
  parameters_setValue( PARAM_BRIGHTNESS, value );
  MOTOR_LED_SET_RED( 1 );
  SERVO_VIBRO_LED_SET_GREEN( 1 );
}

static void get_max_brightness( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_BRIGHTNESS );
}

static void get_min_brightness( uint32_t* value )
{
  *value = 1;
}

static void enter_brightness( void )
{
  MOTOR_LED_SET_RED( 1 );
  SERVO_VIBRO_LED_SET_GREEN( 1 );
}

static void exit_brightness( void )
{
  MOTOR_LED_SET_RED( 0 );
  SERVO_VIBRO_LED_SET_GREEN( 0 );
}

static void get_vibro_duty_pwm( uint32_t* value )
{
  *value = parameters_getValue( PARAM_VIBRO_DUTY_PWM );
}

static void set_vibro_duty_pwm( uint32_t value )
{
  parameters_setValue( PARAM_VIBRO_DUTY_PWM, value );
}

static void get_max_vibro_duty_pwm( uint32_t* value )
{
  *value = parameters_getMaxValue( PARAM_VIBRO_DUTY_PWM );
}

static void get_min_vibro_duty_pwm( uint32_t* value )
{
  *value = parameters_getMinValue( PARAM_VIBRO_DUTY_PWM );
}

const char* get_serial_number( void )
{
  return DevConfig_GetSerialNumber();
}

static void _set_and_exit( menu_token_t* menu )
{
  if ( _state == MENU_EDIT_PARAMETERS )
  {
    if ( parameters_list[menu->position].set_value != NULL )
    {
      parameters_list[menu->position].set_value( parameters_list[menu->position].value );
    }

    if ( parameters_list[menu->position].exit != NULL )
    {
      parameters_list[menu->position].exit();
    }
  }
}

static void _get_values( menu_token_t* menu )
{
  if ( _state == MENU_EDIT_PARAMETERS )
  {
    if ( parameters_list[menu->position].get_value != NULL )
    {
      parameters_list[menu->position].get_value( &parameters_list[menu->position].value );
    }

    if ( parameters_list[menu->position].get_max_value != NULL )
    {
      parameters_list[menu->position].get_max_value( &parameters_list[menu->position].max_value );
    }

    if ( parameters_list[menu->position].get_min_value != NULL )
    {
      parameters_list[menu->position].get_min_value( &parameters_list[menu->position].min_value );
    }

    if ( parameters_list[menu->position].enter != NULL )
    {
      parameters_list[menu->position].enter();
    }

    if ( parameters_list[menu->position].get_str_value != NULL )
    {
      parameters_list[menu->position].str_value = parameters_list[menu->position].get_str_value();
    }
  }
}

static void menu_button_up_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  _set_and_exit( menu );

  menu->last_button = LAST_BUTTON_UP;
  if ( menu->position > 0 )
  {
    menu->position--;
  }

  _get_values( menu );
}

static void menu_button_down_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  _set_and_exit( menu );

  menu->last_button = LAST_BUTTON_DOWN;

  if ( menu->position < parameters_size - 1 )
  {
    menu->position++;
  }

  _get_values( menu );
}

static void menu_button_plus_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  if ( menu->position >= parameters_size )
  {
    return;
  }

  if ( parameters_list[menu->position].value < parameters_list[menu->position].max_value )
  {
    parameters_list[menu->position].value++;
    if ( parameters_list[menu->position].set_value != NULL )
    {
      parameters_list[menu->position].set_value( parameters_list[menu->position].value );
    }
  }
}

static void _fast_add_cb( uint32_t value )
{
  (void) value;
}

static void menu_button_plus_time_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  if ( menu->position >= parameters_size )
  {
    return;
  }

  if ( parameters_list[menu->position].fast_add == NULL )
  {
    fastProcessStart( &parameters_list[menu->position].value, parameters_list[menu->position].max_value, 0, FP_PLUS, _fast_add_cb );
  }
  else
  {
    fastProcessStart( &parameters_list[menu->position].value, parameters_list[menu->position].max_value, 0, FP_PLUS, parameters_list[menu->position].fast_add );
  }
}

static void menu_button_minus_time_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  if ( menu->position >= parameters_size )
  {
    return;
  }

  fastProcessStart( &parameters_list[menu->position].value, parameters_list[menu->position].max_value, parameters_list[menu->position].min_value, FP_MINUS, _fast_add_cb );
}

static void menu_button_m_p_pull_cb( void* arg )
{
  for ( int i = 0; i < parameters_size; i++ )
  {
    fastProcessStop( &parameters_list[i].value );
  }
}

static void menu_button_minus_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  if ( menu->position >= parameters_size )
  {
    return;
  }

  if ( parameters_list[menu->position].value > parameters_list[menu->position].min_value )
  {
    parameters_list[menu->position].value--;
    if ( parameters_list[menu->position].set_value != NULL )
    {
      parameters_list[menu->position].set_value( parameters_list[menu->position].value );
    }
  }
}

static void menu_button_enter_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  if ( menu->position >= parameters_size )
  {
    LOG( PRINT_INFO, "Error settings: menu->position >= parameters_size" );
    return;
  }

  if ( _state == MENU_EDIT_PARAMETERS )
  {
    _state = MENU_LIST_PARAMETERS;
    if ( parameters_list[menu->position].set_value != NULL )
    {
      parameters_list[menu->position].set_value( parameters_list[menu->position].value );
    }

    if ( parameters_list[menu->position].enter != NULL )
    {
      parameters_list[menu->position].exit();
    }
  }
  else
  {
    _state = MENU_EDIT_PARAMETERS;
    _get_values( menu );
  }
}

static void menu_button_exit_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  parameters_save();
  if ( _state == MENU_EDIT_PARAMETERS )
  {
    _set_and_exit( menu );

    _state = MENU_LIST_PARAMETERS;
    return;
  }

  menuDrv_Exit( menu );
}

static bool menu_button_init_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
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

  if ( wifiMenu_GetDevType() == T_DEV_TYPE_SIEWNIK )
  {
    parameters_list = parameters_list_siewnik;
    parameters_size = sizeof( parameters_list_siewnik ) / sizeof( parameters_list_siewnik[0] );
  }
  else
  {
    parameters_list = parameters_list_solarka;
    parameters_size = sizeof( parameters_list_solarka ) / sizeof( parameters_list_solarka[0] );
  }

  return true;
}

static bool menu_enter_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
  }

  _state = MENU_LIST_PARAMETERS;

  return true;
}

static bool menu_exit_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
  }

  return true;
}

static bool menu_process( void* arg )
{
  static char buff[64];
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
  }

  oled_clearScreen();

  switch ( _state )
  {
    case MENU_LIST_PARAMETERS:
      {
        oled_setGLCDFont( OLED_FONT_SIZE_16 );
        oled_printFixed( 2, 0, dictionary_get_string( menu->name_dict ), OLED_FONT_SIZE_16 );
        oled_setGLCDFont( OLED_FONT_SIZE_11 );

        if ( menu->line.end - menu->line.start != MAX_LINE - 1 )
        {
          menu->line.start = menu->position;
          menu->line.end = menu->line.start + MAX_LINE - 1;
        }

        if ( ( menu->position < menu->line.start ) || ( menu->position > menu->line.end ) )
        {
          if ( menu->last_button == LAST_BUTTON_UP )
          {
            menu->line.start = menu->position;
            menu->line.end = menu->line.start + MAX_LINE - 1;
          }
          else
          {
            menu->line.end = menu->position;
            menu->line.start = menu->line.end - MAX_LINE + 1;
          }

          LOG( PRINT_DEBUG, "menu->line.start %d, menu->line.end %d, position %d, menu->last_button %d\n",
               menu->line.start, menu->line.end, menu->position, menu->last_button );
        }

        int line = 0;
        do
        {
          int pos = line + menu->line.start;
          sprintf( buff, "%s", dictionary_get_string( parameters_list[pos].name_dict ) );
          if ( line + menu->line.start == menu->position )
          {
            ssdFigureFillLine( MENU_HEIGHT + LINE_HEIGHT * line, LINE_HEIGHT );
            oled_printFixedBlack( 2, MENU_HEIGHT + LINE_HEIGHT * line, buff, OLED_FONT_SIZE_11 );
          }
          else
          {
            oled_printFixed( 2, MENU_HEIGHT + LINE_HEIGHT * line, buff, OLED_FONT_SIZE_11 );
          }

          line++;
        } while ( line + menu->line.start != parameters_size && line < MAX_LINE );

        scrollBar.actual_line = menu->position;
        scrollBar.all_line = parameters_size - 1;
        ssdFigureDrawScrollBar( &scrollBar );
      }
      break;

    case MENU_EDIT_PARAMETERS:
      oled_printFixed( 2, 0, dictionary_get_string( parameters_list[menu->position].name_dict ), OLED_FONT_SIZE_11 );
      switch ( parameters_list[menu->position].unit_type )
      {
        case UNIT_INT:
          sprintf( buff, "%ld %s", parameters_list[menu->position].value, parameters_list[menu->position].unit_name != NULL ? parameters_list[menu->position].unit_name : "" );
          oled_printFixed( 30, MENU_HEIGHT + 15, buff, OLED_FONT_SIZE_16 );
          break;

        case UNIT_ON_OFF:
          sprintf( buff, "%s", parameters_list[menu->position].value ? dictionary_get_string( DICT_ON ) : dictionary_get_string( DICT_OFF ) );
          oled_printFixed( 30, MENU_HEIGHT + 15, buff, OLED_FONT_SIZE_16 );
          break;

        case UNIT_BOOL:
          sprintf( buff, "%s", parameters_list[menu->position].value ? "1" : "0" );
          oled_printFixed( 30, MENU_HEIGHT + 15, buff, OLED_FONT_SIZE_16 );
          break;

        case UNIT_LANGUAGE:
          sprintf( buff, "%s", language[parameters_list[menu->position].value] );
          oled_printFixed( 30, MENU_HEIGHT + 15, buff, OLED_FONT_SIZE_16 );
          break;

        case UNIT_STR:
          sprintf( buff, "%s", parameters_list[menu->position].str_value );
          oled_printFixed( 30, MENU_HEIGHT + 15, buff, OLED_FONT_SIZE_16 );
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

void menuInitSettingsMenu( menu_token_t* menu )
{
  menu->menu_cb.enter = menu_enter_cb;
  menu->menu_cb.button_init_cb = menu_button_init_cb;
  menu->menu_cb.exit = menu_exit_cb;
  menu->menu_cb.process = menu_process;
}
