#include "app_config.h"
#include "battery.h"
#include "buzzer.h"
#include "cmd_client.h"
#include "dictionary.h"
#include "fast_add.h"
#include "freertos/timers.h"
#include "http_parameters_client.h"
#include "menu_backend.h"
#include "menu_default.h"
#include "menu_drv.h"
#include "oled.h"
#include "parameters.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "start_menu.h"
#include "wifi_menu.h"
#include "wifidrv.h"

#define MODULE_NAME "[START] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_MENU_AUTO
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define CHANGE_MENU_TIMEOUT_MS   1500
#define CHANGE_VALUE_DISP_OFFSET 40

typedef enum
{
  STATE_INIT,
  STATE_CHECK_WIFI,
  STATE_IDLE,
  STATE_START,
  STATE_READY,
  STATE_ERROR,
  STATE_VELOCITY_CHANGE,
  STATE_SERVO_VIBRO_CHANGE,
  STATE_LOW_SILOS,
  STATE_STOP,
  STATE_ERROR_CHECK,
  STATE_RECONNECT,
  STATE_WAIT_CONNECT,
  STATE_TOP,
} state_t;

typedef enum
{
  EDIT_VELOCITY,
  EDIT_SERVO,
  EDIT_TOP,
} edit_value_t;

typedef struct
{
  volatile state_t state;
  bool error_flag;
  bool exit_wait_flag;
  bool enter_parameters_menu;
  const char* error_msg;
  char buff[128];
  char ap_name[64];
  uint32_t timeout_con;
  uint32_t low_silos_ckeck_timeout;
  error_type_t error_dev;
  struct menu_data data;
  TickType_t animation_timeout;
  uint8_t animation_cnt;
  TickType_t change_menu_timeout;
  TickType_t low_silos_timeout;
} menu_start_context_t;

static menu_start_context_t ctx;

__attribute__( ( unused ) ) static char* state_name[] =
  {
    [STATE_INIT] = "STATE_INIT",
    [STATE_IDLE] = "STATE_IDLE",
    [STATE_CHECK_WIFI] = "STATE_CHECK_WIFI",
    [STATE_START] = "STATE_START",
    [STATE_READY] = "STATE_READY",
    [STATE_ERROR] = "STATE_ERROR",
    [STATE_VELOCITY_CHANGE] = "STATE_VELOCITY_CHANGE",
    [STATE_SERVO_VIBRO_CHANGE] = "STATE_SERVO_VIBRO_CHANGE",
    [STATE_LOW_SILOS] = "STATE_LOW_SILOS",
    [STATE_STOP] = "STATE_STOP",
    [STATE_ERROR_CHECK] = "STATE_ERROR_CHECK",
    [STATE_RECONNECT] = "STATE_RECONNECT",
    [STATE_WAIT_CONNECT] = "STATE_WAIT_CONNECT" };

extern void enterMenuParameters( void );

// Group related functions together
// Button callback functions
static void _button_up_callback( void* arg );
static void _button_down_callback( void* arg );
static void _button_exit_callback( void* arg );
static void _button_servo_callback( void* arg );
static void _button_velocity_callback( void* arg );
static void _button_velocity_plus_push_cb( void* arg );
static void _button_velocity_plus_time_cb( void* arg );
static void _button_velocity_minus_push_cb( void* arg );
static void _button_velocity_minus_time_cb( void* arg );
static void _button_velocity_p_m_pull_cb( void* arg );
static void _button_servo_plus_push_cb( void* arg );
static void _button_servo_plus_time_cb( void* arg );
static void _button_servo_minus_push_cb( void* arg );
static void _button_servo_minus_time_cb( void* arg );
static void _button_servo_p_m_pull_cb( void* arg );
static void _button_on_off( void* arg );

// State handling functions
static void _state_init( void );
static void _state_check_connection( void );
static void _state_idle( void );
static void _state_start( void );
static void _state_ready( void );
static void _state_low_silos( void );
static void _state_error( void );
static void _state_velocity_change( void );
static void _state_vibro_change( void );
static void _state_stop( void );
static void _state_error_check( void );
static void _state_reconnect( void );
static void _state_wait_connect( void );

// Helper functions
static void _change_state( state_t new_state );
static void _reset_error( void );
static void _set_change_menu( edit_value_t val );
static bool _is_working_state( void );
static bool _check_low_silos_flag( void );
static void _menu_enter_parameters_callback( void* arg );
static void _velocity_fast_add_cb( uint32_t value );
static void _servo_fast_add_cb( uint32_t value );
static void _show_wait_connection( void );
static void _menu_set_error_msg( const char* msg );

static void reset_error_and_power_save_timer( void )
{
  _reset_error();
}

static void _change_state( state_t new_state )
{
  if ( ctx.state < STATE_TOP )
  {
    if ( ctx.state != new_state )
    {
      LOG( PRINT_DEBUG, "Start menu %s", state_name[new_state] );
    }
    ctx.state = new_state;
  }
  else
  {
    LOG( PRINT_DEBUG, "change state %d", new_state );
  }
}

static void _reset_error( void )
{
  if ( parameters_getValue( PARAM_MACHINE_ERRORS ) )
  {
    HTTPParamClient_SetU32ValueDontWait( PARAM_MACHINE_ERRORS, 0 );
  }
}

static void _set_change_menu( edit_value_t val )
{
  if ( _is_working_state() )
  {
    switch ( val )
    {
      case EDIT_VELOCITY:
        _change_state( STATE_VELOCITY_CHANGE );
        break;
      case EDIT_SERVO:
        _change_state( STATE_SERVO_VIBRO_CHANGE );
        break;

      default:
        return;
    }

    ctx.change_menu_timeout = MS2ST( CHANGE_MENU_TIMEOUT_MS ) + xTaskGetTickCount();
  }
}

static bool _is_working_state( void )
{
  return ( ctx.state == STATE_READY || ctx.state == STATE_SERVO_VIBRO_CHANGE || ctx.state == STATE_VELOCITY_CHANGE || ctx.state == STATE_LOW_SILOS );
}

static bool _check_low_silos_flag( void )
{
  uint32_t flag = parameters_getValue( PARAM_LOW_LEVEL_SILOS );

  LOG( PRINT_DEBUG, "------SILOS FLAG %d---------", flag );
  if ( flag > 0 )
  {
    if ( ctx.low_silos_ckeck_timeout < xTaskGetTickCount() )
    {
      ctx.low_silos_ckeck_timeout = MS2ST( 30000 ) + xTaskGetTickCount();
      _change_state( STATE_LOW_SILOS );
      buzzer_click();
      ctx.low_silos_timeout = MS2ST( 5000 ) + xTaskGetTickCount();
      return true;
    }
  }
  else
  {
    ctx.low_silos_ckeck_timeout = MS2ST( 10000 ) + xTaskGetTickCount();
  }

  return false;
}

static void _menu_enter_parameters_callback( void* arg )
{
  ctx.enter_parameters_menu = true;
  enterMenuParameters();
}

static void _velocity_fast_add_cb( uint32_t value )
{
  (void) value;
  _set_change_menu( EDIT_VELOCITY );
}

static void _servo_fast_add_cb( uint32_t value )
{
  (void) value;
  _set_change_menu( EDIT_SERVO );
}

static void _button_up_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }
}

static void _button_down_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }
}

static void _button_exit_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }
  _reset_error();
  menuDrv_Exit( menu );
  ctx.exit_wait_flag = true;
}

static void _button_servo_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  // ctx.data.servo_vibro_on = ctx.data.servo_vibro_on ? false : true;
}

static void _button_velocity_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  _set_change_menu( EDIT_VELOCITY );

  if ( ctx.data.is_working )
  {
    ctx.data.is_working = false;
    // ctx.data.servo_vibro_on = false;
  }
  else
  {
    ctx.data.is_working = true;
  }
}

static void _button_velocity_plus_push_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  if ( ctx.data.velocity < 200 )
  {
    ctx.data.velocity++;
  }

  _set_change_menu( EDIT_VELOCITY );
}

static void _button_velocity_plus_time_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  fastProcessStart( &ctx.data.velocity, 200, 1, FP_PLUS, _velocity_fast_add_cb );
}

static void _button_velocity_minus_push_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  if ( ctx.data.velocity > 1 )
  {
    ctx.data.velocity--;
  }

  _set_change_menu( EDIT_VELOCITY );
}

static void _button_velocity_minus_time_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  fastProcessStart( &ctx.data.velocity, 200, 1, FP_MINUS, _velocity_fast_add_cb );
}

static void _button_velocity_p_m_pull_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  fastProcessStop( &ctx.data.velocity );

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  menuDrvSaveParameters();
}

/*-------------SERVO BUTTONS------------*/

static void _button_servo_plus_push_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  if ( ctx.data.kg_per_ha < 100 )
  {
    ctx.data.kg_per_ha++;
  }

  _set_change_menu( EDIT_SERVO );
}

static void _button_servo_plus_time_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  fastProcessStart( &ctx.data.kg_per_ha, 100, 0, FP_PLUS, _servo_fast_add_cb );
}

static void _button_servo_minus_push_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  if ( ctx.data.kg_per_ha > 0 )
  {
    ctx.data.kg_per_ha--;
  }

  _set_change_menu( EDIT_SERVO );
}

static void _button_servo_minus_time_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  fastProcessStart( &ctx.data.kg_per_ha, 100, 0, FP_MINUS, _servo_fast_add_cb );
}

static void _button_servo_p_m_pull_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  fastProcessStop( &ctx.data.kg_per_ha );

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  menuDrvSaveParameters();
}

static void _button_on_off( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  _reset_error();
}

static bool menu_button_init_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
  }

  menu->button.down.fall_callback = _button_down_callback;
  menu->button.down.timer_callback = _menu_enter_parameters_callback;
  menu->button.up.timer_callback = _menu_enter_parameters_callback;
  menu->button.up.fall_callback = _button_up_callback;
  menu->button.enter.fall_callback = _button_exit_callback;
  menu->button.exit.fall_callback = _button_servo_callback;

  menu->button.up_minus.fall_callback = _button_velocity_minus_push_cb;
  menu->button.up_minus.rise_callback = _button_velocity_p_m_pull_cb;
  menu->button.up_minus.timer_callback = _button_velocity_minus_time_cb;
  menu->button.up_plus.fall_callback = _button_velocity_plus_push_cb;
  menu->button.up_plus.rise_callback = _button_velocity_p_m_pull_cb;
  menu->button.up_plus.timer_callback = _button_velocity_plus_time_cb;

  menu->button.down_minus.fall_callback = _button_servo_minus_push_cb;
  menu->button.down_minus.rise_callback = _button_servo_p_m_pull_cb;
  menu->button.down_minus.timer_callback = _button_servo_minus_time_cb;
  menu->button.down_plus.fall_callback = _button_servo_plus_push_cb;
  menu->button.down_plus.rise_callback = _button_servo_p_m_pull_cb;
  menu->button.down_plus.timer_callback = _button_servo_plus_time_cb;

  menu->button.is_working.fall_callback = _button_velocity_callback;
  menu->button.on_off.fall_callback = _button_on_off;
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

  if ( !backendIsConnected() )
  {
    _change_state( STATE_INIT );
  }

  HTTPParamClient_SetU32ValueDontWait( PARAM_START_SYSTEM, 1 );

  ctx.data.velocity = parameters_getValue( PARAM_VELOCITY );
  ctx.data.kg_per_ha = parameters_getValue( PARAM_GRAIN_PER_HECTARE );
  ctx.data.is_working = parameters_getValue( PARAM_MOTOR_IS_ON );
  // ctx.data.servo_vibro_on = parameters_getValue( PARAM_GRAIN_PER_HECTARE_IS_ON );
  if ( !ctx.enter_parameters_menu )
  {
    ctx.data.is_working = 0;
    // ctx.data.servo_vibro_on = 0;
  }

  HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR, parameters_getValue( PARAM_ERROR_MOTOR ) );
  HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_SERVO, parameters_getValue( PARAM_ERROR_SERVO ) );
  HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR_CALIBRATION, parameters_getValue( PARAM_ERROR_MOTOR_CALIBRATION ) );
  HTTPParamClient_SetU32ValueDontWait( PARAM_SILOS_HEIGHT, parameters_getValue( PARAM_SILOS_HEIGHT ) );
  backendEnterMenuStart();

  ctx.error_flag = 0;
  ctx.enter_parameters_menu = false;
  return true;
}

static bool menu_exit_cb( void* arg )
{
  if ( !ctx.enter_parameters_menu )
  {
    ctx.data.is_working = 0;
    // ctx.data.servo_vibro_on = 0;
  }

  backendExitMenuStart();

  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
  }

  MOTOR_LED_SET_GREEN( 0 );
  SERVO_VIBRO_LED_SET_GREEN( 0 );
  MOTOR_LED_SET_RED( 0 );
  SERVO_VIBRO_LED_SET_RED( 0 );
  return true;
}

static void _menu_set_error_msg( const char* msg )
{
  ctx.error_msg = msg;
  ctx.error_flag = 1;
  _change_state( STATE_ERROR_CHECK );
}

static void _state_init( void )
{
  oled_printFixed( 2, 2 * LINE_HEIGHT, dictionary_get_string( DICT_CHECK_CONNECTION ), OLED_FONT_SIZE_11 );
  _change_state( STATE_CHECK_WIFI );
}

static void _state_check_connection( void )
{
  if ( !backendIsConnected() )
  {
    _change_state( STATE_RECONNECT );
    return;
  }

  bool ret = false;

  ctx.data.velocity = parameters_getValue( PARAM_VELOCITY );
  ctx.data.kg_per_ha = parameters_getValue( PARAM_GRAIN_PER_HECTARE );
  ctx.data.is_working = 0;
  // ctx.data.servo_vibro_on = 0;
  for ( uint8_t i = 0; i < 3; i++ )
  {
    LOG( PRINT_INFO, "START_MENU: cmdClientGetAllValue try %d", i );
    osDelay( 250 );

    if ( ( HTTPParamClient_SetU32Value( PARAM_EMERGENCY_DISABLE, 0, 1000 ) == ERROR_CODE_OK ) && ( HTTPParamClient_SetU32Value( PARAM_PERIOD, parameters_getValue( PARAM_PERIOD ), 1000 ) == ERROR_CODE_OK ) )
    {
      ret = true;
      break;
    }
  }

  if ( ret != TRUE )
  {
    LOG( PRINT_INFO, "%s: error get parameters", __func__ );
    ctx.data.is_working = 0;
    // ctx.data.servo_vibro_on = 0;
    _change_state( STATE_RECONNECT );
    return;
  }

  _change_state( STATE_IDLE );
}

static void _state_idle( void )
{
  if ( backendIsConnected() )
  {
    HTTPParamClient_SetU32ValueDontWait( PARAM_START_SYSTEM, 1 );
    ctx.data.is_working = 0;
    ctx.data.velocity = parameters_getValue( PARAM_VELOCITY );
    ctx.data.kg_per_ha = parameters_getValue( PARAM_GRAIN_PER_HECTARE );
    // ctx.data.servo_vibro_on = 0;
    HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR, parameters_getValue( PARAM_ERROR_MOTOR ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_SERVO, parameters_getValue( PARAM_ERROR_SERVO ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR_CALIBRATION, parameters_getValue( PARAM_ERROR_MOTOR_CALIBRATION ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_SILOS_HEIGHT, parameters_getValue( PARAM_SILOS_HEIGHT ) );
    _change_state( STATE_START );
  }
  else
  {
    menuPrintfInfo( "   Target not connected.\n        Go to DEVICES\n         for connect" );
  }
}

static void _state_start( void )
{
  if ( !backendIsConnected() )
  {
    return;
  }

  _change_state( STATE_READY );
}

static void _state_ready( void )
{
  if ( !backendIsConnected() )
  {
    ctx.data.is_working = 0;
    ctx.data.velocity = parameters_getValue( PARAM_VELOCITY );
    ctx.data.kg_per_ha = parameters_getValue( PARAM_GRAIN_PER_HECTARE );
    // ctx.data.servo_vibro_on = 0;
    HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR, parameters_getValue( PARAM_ERROR_MOTOR ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_SERVO, parameters_getValue( PARAM_ERROR_SERVO ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR_CALIBRATION, parameters_getValue( PARAM_ERROR_MOTOR_CALIBRATION ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_SILOS_HEIGHT, parameters_getValue( PARAM_SILOS_HEIGHT ) );
    _change_state( STATE_START );
  }
  else
  {
    menuPrintfInfo( "   Target not connected.\n        Go to DEVICES\n         for connect" );
  }
}

static void _state_start( void )
{
  if ( !backendIsConnected() )
  {
    return;
  }

  _change_state( STATE_READY );
}

static void _state_ready( void )
{
  if ( !backendIsConnected() )
  {
    _menu_set_error_msg( dictionary_get_string( DICT_LOST_CONNECTION_WITH_SERVER ) );
    return;
  }

  backendEnterMenuStart();

  if ( _check_low_silos_flag() )
  {
    return;
  }

  if ( ctx.animation_timeout < xTaskGetTickCount() )
  {
    ctx.animation_cnt++;
    ctx.animation_timeout = xTaskGetTickCount() + MS2ST( 100 );
  }

  char str[32];
  sprintf( str, "%d km/h", ctx.data.velocity );
  oled_clearScreen();
  oled_printFixed( 70, 22, str, OLED_FONT_SIZE_11 );
  uint8_t cnt = 0;

  if ( ctx.data.is_working )
  {
    cnt = ctx.animation_cnt % 6;
  }

  drawMotorCircle( 5, 2, cnt );

  if ( wifiMenu_GetDevType() == T_DEV_TYPE_SIEWNIK )
  {
    ssdFigure_DrawLowAccu( 60, 1, parameters_getValue( PARAM_VOLTAGE_ACCUM ), parameters_getValue( PARAM_CURRENT_MOTOR ) );

    if ( parameters_getValue( PARAM_SILOS_SENSOR_IS_CONNECTED ) )
    {
      uint32_t silos_level = parameters_getValue( PARAM_SILOS_LEVEL );
      sprintf( str, "%ld", parameters_getValue( PARAM_SILOS_LEVEL ) );
      if ( silos_level > 99 )
      {
        oled_printFixed( 10, 10, str, OLED_FONT_SIZE_11 );
      }
      else if ( silos_level < 100 && silos_level > 9 )
      {
        oled_printFixed( 14, 10, str, OLED_FONT_SIZE_11 );
      }
      else if ( silos_level < 10 )
      {
        oled_printFixed( 18, 10, str, OLED_FONT_SIZE_11 );
      }
    }
    sprintf( str, "%d%%", ctx.data.kg_per_ha );
    oled_printFixed( 70, 52, str, OLED_FONT_SIZE_11 );
  }
  else
  {
    oled_printFixed( 2, 3 * LINE_HEIGHT, "Unsupported\ndevice type", OLED_FONT_SIZE_11 );
  }
}

static void _state_low_silos( void )
{
  if ( !backendIsConnected() )
  {
    _menu_set_error_msg( dictionary_get_string( DICT_LOST_CONNECTION_WITH_SERVER ) );
    return;
  }

  if ( ctx.low_silos_timeout < xTaskGetTickCount() )
  {
    _change_state( STATE_READY );
    return;
  }

  oled_clearScreen();
  oled_printFixed( 5, 6, dictionary_get_string( DICT_LOW ), OLED_FONT_SIZE_26 );
  // oled_printFixed(24, 30, dictionary_get_string(DICT_SILOS), OLED_FONT_SIZE_26);
}

static void _state_error( void )
{
  static uint32_t blink_counter;
  static bool blink_state;
  bool motor_led_blink = false;
  bool servo_led_blink = false;

  MOTOR_LED_SET_GREEN( 0 );
  SERVO_VIBRO_LED_SET_GREEN( 0 );

  if ( !backendIsConnected() )
  {
    _menu_set_error_msg( dictionary_get_string( DICT_LOST_CONNECTION_WITH_SERVER ) );
    return;
  }

  switch ( ctx.error_dev )
  {
    case ERROR_SERVO_NOT_CONNECTED:
      oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_SERVO_NOT_CONNECTED ), OLED_FONT_SIZE_16 );
      break;

    case ERROR_SERVO_OVER_CURRENT:
      oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_SERVO_OVERCURRENT ), OLED_FONT_SIZE_16 );
      break;

    case ERROR_MOTOR_NOT_CONNECTED:
      oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_MOTOR_NOT_CONNECTED ), OLED_FONT_SIZE_16 );
      motor_led_blink = true;
      break;

    case ERROR_VIBRO_NOT_CONNECTED:
      oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_VIBRO_NOT_CONNECTED ), OLED_FONT_SIZE_16 );
      servo_led_blink = true;
      break;

    case ERROR_VIBRO_OVER_CURRENT:
      oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_VIBRO_OVERCURRENT ), OLED_FONT_SIZE_16 );
      servo_led_blink = true;
      break;

    case ERROR_MOTOR_OVER_CURRENT:
      oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_MOTOR_OVERCURRENT ), OLED_FONT_SIZE_16 );
      motor_led_blink = true;
      break;

    case ERROR_OVER_TEMPERATURE:
      menuPrintfInfo( dictionary_get_string( DICT_TEMPERATURE_IS_HIGH ) );
      motor_led_blink = true;
      servo_led_blink = true;
      break;

    default:
      menuPrintfInfo( dictionary_get_string( DICT_UNKNOWN_ERROR ) );
      motor_led_blink = true;
      servo_led_blink = true;
      break;
  }

  if ( ( blink_counter++ ) % 2 == 0 )
  {
    MOTOR_LED_SET_RED( motor_led_blink ? blink_state : 0 );
    SERVO_VIBRO_LED_SET_RED( servo_led_blink ? blink_state : 0 );
    blink_state = blink_state ? false : true;
  }
}

static void _state_velocity_change( void )
{
  if ( !backendIsConnected() )
  {
    _menu_set_error_msg( dictionary_get_string( DICT_LOST_CONNECTION_WITH_SERVER ) );
    return;
  }
  ssdFigure_DrawLowAccu( 60, 1, parameters_getValue( PARAM_VOLTAGE_ACCUM ), parameters_getValue( PARAM_CURRENT_MOTOR ) );
  oled_printFixed( 0, 0, dictionary_get_string( DICT_SPEED ), OLED_FONT_SIZE_26 );
  sprintf( ctx.buff, "%ld km/h", ctx.data.velocity );
  oled_printFixed( CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT, ctx.buff, OLED_FONT_SIZE_26 );    // Font_16x26

  if ( ctx.change_menu_timeout < xTaskGetTickCount() )
  {
    _change_state( STATE_READY );
  }
}

static void _state_vibro_change( void )
{
  ssdFigure_DrawLowAccu( 60, 1, parameters_getValue( PARAM_VOLTAGE_ACCUM ), parameters_getValue( PARAM_CURRENT_MOTOR ) );

  if ( !backendIsConnected() )
  {
    _menu_set_error_msg( dictionary_get_string( DICT_LOST_CONNECTION_WITH_SERVER ) );
    return;
  }

  if ( wifiMenu_GetDevType() == T_DEV_TYPE_SOLARKA )
  {
    oled_printFixed( 0, 0, dictionary_get_string( DICT_VIBRO_ON ), OLED_FONT_SIZE_26 );
    sprintf( ctx.buff, "%ld%%", ctx.data.kg_per_ha );
    oled_printFixed( CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT, ctx.buff, OLED_FONT_SIZE_26 );
  }
  else
  {
    ssdFigure_DrawLowAccu( 60, 1, parameters_getValue( PARAM_VOLTAGE_ACCUM ), parameters_getValue( PARAM_CURRENT_MOTOR ) );
    oled_printFixed( 0, 0, dictionary_get_string( DICT_SERVO ), OLED_FONT_SIZE_26 );
    sprintf( ctx.buff, "%ld%%", ctx.data.kg_per_ha );
    oled_printFixed( CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT, ctx.buff, OLED_FONT_SIZE_26 );
  }

  if ( ctx.change_menu_timeout < xTaskGetTickCount() )
  {
    _change_state( STATE_READY );
  }
}

static void _state_stop( void )
{
}

static void _state_error_check( void )
{
  if ( ctx.error_flag )
  {
    menuPrintfInfo( ctx.error_msg );
    ctx.error_flag = false;
  }
  else
  {
    _change_state( STATE_INIT );
    osDelay( 700 );
  }
}

static void _state_reconnect( void )
{
  backendExitMenuStart();

  wifiDrvGetAPName( ctx.ap_name );
  if ( strlen( ctx.ap_name ) > 5 )
  {
    wifiDrvConnect();
    _change_state( STATE_WAIT_CONNECT );
  }
}

static void _show_wait_connection( void )
{
  oled_clearScreen();
  sprintf( ctx.buff, dictionary_get_string( DICT_WAIT_CONNECTION_S_S_S ), xTaskGetTickCount() % 400 > 100 ? "." : " ",
           xTaskGetTickCount() % 400 > 200 ? "." : " ", xTaskGetTickCount() % 400 > 300 ? "." : " " );
  oled_printFixed( 2, 2 * LINE_HEIGHT, ctx.buff, OLED_FONT_SIZE_11 );
  oled_update();
}

static void _state_wait_connect( void )
{
  /* Wait to connect wifi */
  ctx.timeout_con = MS2ST( 10000 ) + xTaskGetTickCount();
  ctx.exit_wait_flag = false;
  do
  {
    if ( ( ctx.timeout_con < xTaskGetTickCount() ) || ctx.exit_wait_flag )
    {
      _menu_set_error_msg( dictionary_get_string( DICT_TIMEOUT_CONNECT ) );
      return;
    }

    _show_wait_connection();
    osDelay( 50 );
  } while ( wifiDrvTryingConnect() );

  ctx.timeout_con = MS2ST( 10000 ) + xTaskGetTickCount();
  do
  {
    if ( ( ctx.timeout_con < xTaskGetTickCount() ) || ctx.exit_wait_flag )
    {
      _menu_set_error_msg( dictionary_get_string( DICT_TIMEOUT_SERVER ) );
      return;
    }

    _show_wait_connection();
    osDelay( 50 );
  } while ( !backendIsConnected() );

  oled_clearScreen();
  menuPrintfInfo( dictionary_get_string( DICT_CONNECTED_TRY_READ_DATA ) );
  _change_state( STATE_CHECK_WIFI );
}

static bool menu_process( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
  }

  switch ( ctx.state )
  {
    case STATE_INIT:
      _state_init();
      break;

    case STATE_CHECK_WIFI:
      _state_check_connection();
      break;

    case STATE_IDLE:
      _state_idle();
      break;

    case STATE_START:
      _state_start();
      break;

    case STATE_READY:
      _state_ready();
      break;

    case STATE_ERROR:
      _state_error();
      break;

    case STATE_VELOCITY_CHANGE:
      _state_velocity_change();
      break;

    case STATE_SERVO_VIBRO_CHANGE:
      _state_vibro_change();
      break;

    case STATE_LOW_SILOS:
      _state_low_silos();
      break;

    case STATE_STOP:
      _state_stop();
      break;

    case STATE_ERROR_CHECK:
      _state_error_check();
      break;

    case STATE_RECONNECT:
      _state_reconnect();
      break;

    case STATE_WAIT_CONNECT:
      _state_wait_connect();
      break;

    default:
      _change_state( STATE_STOP );
      break;
  }

  if ( backendIsEmergencyDisable() || ctx.state == STATE_ERROR || !backendIsConnected() )
  {
    MOTOR_LED_SET_GREEN( 0 );
    SERVO_VIBRO_LED_SET_GREEN( 0 );
  }
  else
  {
    MOTOR_LED_SET_GREEN( ctx.data.is_working );
    // SERVO_VIBRO_LED_SET_GREEN( ctx.data.servo_vibro_on );
    MOTOR_LED_SET_RED( 0 );
    SERVO_VIBRO_LED_SET_RED( 0 );
  }

  return true;
}

void menuStartReset( void )
{
  ctx.data.is_working = false;
  // ctx.data.servo_vibro_on = false;
}

void menuInitStartMenu( menu_token_t* menu )
{
  memset( &ctx, 0, sizeof( ctx ) );
  menu->menu_cb.enter = menu_enter_cb;
  menu->menu_cb.button_init_cb = menu_button_init_cb;
  menu->menu_cb.exit = menu_exit_cb;
  menu->menu_cb.process = menu_process;
}

void menuStartSetError( error_type_t error )
{
  LOG( PRINT_DEBUG, "%s %d", __func__, error );
  ctx.error_dev = error;
  if ( ctx.state == STATE_READY || ctx.state == STATE_VELOCITY_CHANGE || ctx.state == STATE_SERVO_VIBRO_CHANGE )
  {
    _change_state( STATE_ERROR );
  }
  ctx.data.is_working = false;
  // ctx.data.servo_vibro_on = false;
}

void menuStartResetError( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  if ( ctx.state == STATE_ERROR )
  {
    ctx.error_dev = ERROR_TOP;
    _change_state( STATE_READY );
  }
}

struct menu_data* menuStartGetData( void )
{
  return &ctx.data;
}