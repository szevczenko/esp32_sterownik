#include "menu_auto.h"

#include "app_config.h"
#include "battery.h"
#include "buzzer.h"
#include "cmd_client.h"
#include "dictionary.h"
#include "e108_position_driver.h"
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
#include "wifi_menu.h"
#include "wifidrv.h"

#define MODULE_NAME "[START] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_MENU_AUTO
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define CHANGE_MENU_TIMEOUT_MS        1500
#define CHANGE_VALUE_DISP_OFFSET      40
#define VELOCITY_WARNING_TIMEOUT_MS   5000    // 5 seconds
#define SIMULTANEOUS_PRESS_TIMEOUT_MS 1000    // Time in ms to detect simultaneous press

typedef enum
{
  STATE_INIT,
  STATE_CHECK_WIFI,
  STATE_IDLE,
  STATE_READY,
  STATE_ERROR,
  STATE_VELOCITY_CHANGE,
  STATE_KG_PER_HA_CHANGE,
  STATE_LOW_SILOS,
  STATE_STOP,
  STATE_ERROR_CHECK,
  STATE_RECONNECT,
  STATE_WAIT_CONNECT,
  STATE_VELOCITY_WARNING,    // Add this line
  STATE_MOTOR_CHANGE,    // Add this state
  STATE_TOP,
} state_t;

typedef enum
{
  EDIT_VELOCITY,
  EDIT_SERVO,
  EDIT_MOTOR,    // Add this edit value type
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
  uint32_t low_silos_check_timeout;
  error_type_t error_dev;
  struct auto_data data;
  TickType_t animation_timeout;
  uint8_t animation_cnt;
  TickType_t change_menu_timeout;
  TickType_t low_silos_timeout;
  TickType_t velocity_warning_timeout;    // Add this line
  TickType_t velocity_warning_msg_time;
  TickType_t simultaneous_press_time;
  bool velocity_warning_triggered;    // Add this line
  bool button_up_pressed;    // Add this line
  bool button_down_pressed;    // Add this line
  bool both_buttons_pressed;
  float velocity;
  float distance_km;
  e108_gnss_status_t velocity_sensor_status;
} menu_start_context_t;

static menu_start_context_t ctx;

__attribute__( ( unused ) ) static char* state_name[] =
  {
    [STATE_INIT] = "STATE_INIT",
    [STATE_IDLE] = "STATE_IDLE",
    [STATE_CHECK_WIFI] = "STATE_CHECK_WIFI",
    [STATE_READY] = "STATE_READY",
    [STATE_ERROR] = "STATE_ERROR",
    [STATE_VELOCITY_CHANGE] = "STATE_VELOCITY_CHANGE",
    [STATE_KG_PER_HA_CHANGE] = "STATE_KG_PER_HA_CHANGE",
    [STATE_LOW_SILOS] = "STATE_LOW_SILOS",
    [STATE_STOP] = "STATE_STOP",
    [STATE_ERROR_CHECK] = "STATE_ERROR_CHECK",
    [STATE_RECONNECT] = "STATE_RECONNECT",
    [STATE_WAIT_CONNECT] = "STATE_WAIT_CONNECT",
    [STATE_VELOCITY_WARNING] = "STATE_VELOCITY_WARNING",
    [STATE_MOTOR_CHANGE] = "STATE_MOTOR_CHANGE" };    // Add this to state_name

extern void enterMenuParameters( void );
extern void backendEnterMenuAuto( void );    // Add this line
extern void backendExitMenuAuto( void );    // Add this line

// Group related functions together
// Button callback functions
static void _button_up_callback( void* arg );
static void _button_up_release_callback( void* arg );    // Add this line
static void _button_up_timer_callback( void* arg );    // Add this line
static void _button_down_callback( void* arg );
static void _button_down_release_callback( void* arg );    // Add this line
static void _button_down_timer_callback( void* arg );    // Add this line
static void _button_exit_callback( void* arg );
static void _button_reset_distance_callback( void* arg );
static void _button_motor_callback( void* arg );
static void _button_motor_plus_push_cb( void* arg );
static void _button_motor_plus_time_cb( void* arg );
static void _button_motor_minus_push_cb( void* arg );
static void _button_motor_minus_time_cb( void* arg );
static void _button_motor_p_m_pull_cb( void* arg );
static void _button_kg_per_ha_plus_push_cb( void* arg );
static void _button_kg_per_ha_plus_time_cb( void* arg );
static void _button_kg_per_ha_minus_push_cb( void* arg );
static void _button_kg_per_ha_minus_time_cb( void* arg );
static void _button_kg_per_ha_p_m_pull_cb( void* arg );
static void _button_on_off( void* arg );

// State handling functions
static void _state_init( void );
static void _state_check_connection( void );
static void _state_idle( void );
static void _state_ready( void );
static void _state_low_silos( void );
static void _state_error( void );
static void _state_velocity_change( void );
static void _state_kg_per_ha_change( void );
static void _state_stop( void );
static void _state_error_check( void );
static void _state_reconnect( void );
static void _state_wait_connect( void );
static void _state_velocity_warning( void );    // Add this line
static void _state_motor_change( void );    // Add this line

// Helper functions
static void _change_state( state_t new_state );
static void _reset_error( void );
static void _set_change_menu( edit_value_t val );
static bool _is_working_state( void );
static bool _check_low_silos_flag( void );
static void _velocity_fast_add_cb( uint32_t value );
static void _servo_fast_add_cb( uint32_t value );
static void _motor_fast_add_cb( uint32_t value );    // Add this line
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
        _change_state( STATE_KG_PER_HA_CHANGE );
        break;
      case EDIT_MOTOR:    // Add this case
        _change_state( STATE_MOTOR_CHANGE );
        break;
      default:
        return;
    }

    ctx.change_menu_timeout = MS2ST( CHANGE_MENU_TIMEOUT_MS ) + xTaskGetTickCount();
  }
}

static bool _is_working_state( void )
{
  return ( ctx.state == STATE_READY || ctx.state == STATE_KG_PER_HA_CHANGE || ctx.state == STATE_VELOCITY_CHANGE || ctx.state == STATE_LOW_SILOS || ctx.state == STATE_MOTOR_CHANGE );
}

static bool _check_low_silos_flag( void )
{
  uint32_t flag = parameters_getValue( PARAM_LOW_LEVEL_SILOS );
  uint32_t silos_level = parameters_getValue( PARAM_SILOS_LEVEL );
  static uint8_t low_silos_counter = 0;
  static uint32_t cycle_counter = 0;
  static bool has_entered = false;

  cycle_counter++;

  if ( !has_entered && cycle_counter < 20 )
  {
    return false;    // Nie wykonuj funkcji, jeśli cykle < 20 za pierwszym razem
  }

  if ( flag > 0 )
  {
    if ( ctx.low_silos_check_timeout < xTaskGetTickCount() )
    {
      if ( low_silos_counter >= 3 && silos_level <= 10 )
      {
        return false;
      }

      ctx.low_silos_check_timeout = MS2ST( 30000 ) + xTaskGetTickCount();
      _change_state( STATE_LOW_SILOS );
      buzzer_click();
      vTaskDelay( MS2ST( 150 ) );
      buzzer_click();
      ctx.low_silos_timeout = MS2ST( 1500 ) + xTaskGetTickCount();
      low_silos_counter++;
      has_entered = true;
      cycle_counter = 0;
      return true;
    }
  }
  else
  {
    ctx.low_silos_check_timeout = MS2ST( 10000 ) + xTaskGetTickCount();
    low_silos_counter = 0;    // Reset counter when flag is not set
    has_entered = false;
  }

  return false;
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

static void _motor_fast_add_cb( uint32_t value )    // Add this callback
{
  (void) value;
  _set_change_menu( EDIT_MOTOR );
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
  ctx.button_up_pressed = true;    // Set button state to pressed

  if ( !_is_working_state() )
  {
    return;
  }

  // If down button is also pressed, mark for simultaneous press
  if ( ctx.button_down_pressed )    // Use stored state instead of Button_GetState
  {
    ctx.both_buttons_pressed = true;
    ctx.simultaneous_press_time = xTaskGetTickCount() + MS2ST( SIMULTANEOUS_PRESS_TIMEOUT_MS );
    fastProcessStop( &ctx.data.set_velocity );    // Stop fast process immediately
    return;
  }

  // Otherwise increment set_velocity
  if ( ctx.data.set_velocity < parameters_getMaxValue( PARAM_SET_VELOCITY_KM_H ) )
  {
    ctx.data.set_velocity++;
    _set_change_menu( EDIT_VELOCITY );
  }
}

static void _button_up_release_callback( void* arg )    // Add this function
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  ctx.button_up_pressed = false;    // Set button state to released
  fastProcessStop( &ctx.data.set_velocity );    // Stop fast process if active
}

static void _button_up_timer_callback( void* arg )    // Add this function
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

  fastProcessStart( &ctx.data.set_velocity, parameters_getMaxValue( PARAM_SET_VELOCITY_KM_H ), 1, FP_PLUS, _velocity_fast_add_cb );
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
  ctx.button_down_pressed = true;    // Set button state to pressed

  if ( !_is_working_state() )
  {
    return;
  }

  // If up button is also pressed, mark for simultaneous press
  if ( ctx.button_up_pressed )    // Use stored state instead of Button_GetState
  {
    ctx.both_buttons_pressed = true;
    ctx.simultaneous_press_time = xTaskGetTickCount() + MS2ST( SIMULTANEOUS_PRESS_TIMEOUT_MS );
    fastProcessStop( &ctx.data.set_velocity );    // Stop fast process immediately
    return;
  }

  // Otherwise decrement set_velocity
  if ( ctx.data.set_velocity > 1 )    // Assume 1 is the minimum value
  {
    ctx.data.set_velocity--;
    _set_change_menu( EDIT_VELOCITY );
  }
}

static void _button_down_release_callback( void* arg )    // Add this function
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  ctx.button_down_pressed = false;    // Set button state to released
  fastProcessStop( &ctx.data.set_velocity );    // Stop fast process if active
}

static void _button_down_timer_callback( void* arg )    // Add this function
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

  fastProcessStart( &ctx.data.set_velocity, parameters_getMaxValue( PARAM_SET_VELOCITY_KM_H ), 1, FP_MINUS, _velocity_fast_add_cb );
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

static void _button_reset_distance_callback( void* arg )
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

  HTTPParamClient_SetU32ValueDontWait( PARAM_RESET_DISTANCE, 1 );
}

static void _button_motor_callback( void* arg )
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

static void _button_motor_plus_push_cb( void* arg )
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

  if ( ctx.data.motor_rpm < parameters_getMaxValue( PARAM_MOTOR_RPM_PER_100 ) / 100 )    // Changed to motor_rpm
  {
    ctx.data.motor_rpm++;
  }

  _set_change_menu( EDIT_MOTOR );    // Change to EDIT_MOTOR
}

static void _button_motor_plus_time_cb( void* arg )
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

  fastProcessStart( &ctx.data.motor_rpm, parameters_getMaxValue( PARAM_MOTOR_RPM_PER_100 ) / 100, 1, FP_PLUS, _motor_fast_add_cb );    // Changed to motor_rpm
}

static void _button_motor_minus_push_cb( void* arg )
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

  if ( ctx.data.motor_rpm > 0 )    // Changed to motor_rpm
  {
    ctx.data.motor_rpm--;
  }

  _set_change_menu( EDIT_MOTOR );
}

static void _button_motor_minus_time_cb( void* arg )
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

  fastProcessStart( &ctx.data.motor_rpm, parameters_getMaxValue( PARAM_MOTOR_RPM_PER_100 ) / 100, 0, FP_MINUS, _motor_fast_add_cb );    // Changed to motor_rpm
}

static void _button_motor_p_m_pull_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  fastProcessStop( &ctx.data.motor_rpm );    // Changed to motor_rpm

  reset_error_and_power_save_timer();

  if ( !_is_working_state() )
  {
    return;
  }

  menuDrvSaveParameters();
}

/*-------------SERVO BUTTONS------------*/

static void _button_kg_per_ha_plus_push_cb( void* arg )
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

  if ( ctx.data.kg_per_ha < parameters_getMaxValue( PARAM_GRAIN_PER_HECTARE ) )
  {
    ctx.data.kg_per_ha++;
  }

  _set_change_menu( EDIT_SERVO );
}

static void _button_kg_per_ha_plus_time_cb( void* arg )
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

  fastProcessStart( &ctx.data.kg_per_ha, parameters_getMaxValue( PARAM_GRAIN_PER_HECTARE ), 0, FP_PLUS, _servo_fast_add_cb );
}

static void _button_kg_per_ha_minus_push_cb( void* arg )
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

static void _button_kg_per_ha_minus_time_cb( void* arg )
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

  fastProcessStart( &ctx.data.kg_per_ha, parameters_getMaxValue( PARAM_GRAIN_PER_HECTARE ), 0, FP_MINUS, _servo_fast_add_cb );
}

static void _button_kg_per_ha_p_m_pull_cb( void* arg )
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

  // Register our modified callbacks
  menu->button.down.fall_callback = _button_down_callback;
  menu->button.down.rise_callback = _button_down_release_callback;    // Add rise callback
  menu->button.down.timer_callback = _button_down_timer_callback;    // Update timer callback

  menu->button.up.fall_callback = _button_up_callback;
  menu->button.up.rise_callback = _button_up_release_callback;    // Add rise callback
  menu->button.up.timer_callback = _button_up_timer_callback;    // Update timer callback

  menu->button.enter.fall_callback = _button_exit_callback;
  menu->button.exit.fall_callback = _button_reset_distance_callback;

  menu->button.up_minus.fall_callback = _button_motor_minus_push_cb;
  menu->button.up_minus.rise_callback = _button_motor_p_m_pull_cb;
  menu->button.up_minus.timer_callback = _button_motor_minus_time_cb;
  menu->button.up_plus.fall_callback = _button_motor_plus_push_cb;
  menu->button.up_plus.rise_callback = _button_motor_p_m_pull_cb;
  menu->button.up_plus.timer_callback = _button_motor_plus_time_cb;

  menu->button.down_minus.fall_callback = _button_kg_per_ha_minus_push_cb;
  menu->button.down_minus.rise_callback = _button_kg_per_ha_p_m_pull_cb;
  menu->button.down_minus.timer_callback = _button_kg_per_ha_minus_time_cb;
  menu->button.down_plus.fall_callback = _button_kg_per_ha_plus_push_cb;
  menu->button.down_plus.rise_callback = _button_kg_per_ha_p_m_pull_cb;
  menu->button.down_plus.timer_callback = _button_kg_per_ha_plus_time_cb;

  menu->button.motor_on.fall_callback = _button_motor_callback;
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

  ctx.data.set_velocity = parameters_getValue( PARAM_SET_VELOCITY_KM_H );
  ctx.data.kg_per_ha = parameters_getValue( PARAM_GRAIN_PER_HECTARE );
  LOG( PRINT_INFO, "%s: PARAM_GRAIN_PER_HECTARE %d", __func__, parameters_getValue( PARAM_GRAIN_PER_HECTARE ) );
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
  HTTPParamClient_SetU32ValueDontWait( PARAM_SILOS_HEIGHT_CM, parameters_getValue( PARAM_SILOS_HEIGHT_CM ) );
  HTTPParamClient_SetU32ValueDontWait( PARAM_HIGH_OF_MACHINE_CM, parameters_getValue( PARAM_HIGH_OF_MACHINE_CM ) );

  backendEnterMenuAuto();

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

  backendExitMenuAuto();

  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
  }

  MOTOR_LED_SET_GREEN( 0 );
  //  SERVO_VIBRO_LED_SET_GREEN( 0 );
  MOTOR_LED_SET_RED( 0 );
  //  SERVO_VIBRO_LED_SET_RED( 0 );
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

  ctx.data.kg_per_ha = parameters_getValue( PARAM_GRAIN_PER_HECTARE );
  LOG( PRINT_INFO, "%s: PARAM_GRAIN_PER_HECTARE %d", __func__, parameters_getValue( PARAM_GRAIN_PER_HECTARE ) );
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
    ctx.data.kg_per_ha = parameters_getValue( PARAM_GRAIN_PER_HECTARE );
    LOG( PRINT_INFO, "%s: PARAM_GRAIN_PER_HECTARE %d", __func__, parameters_getValue( PARAM_GRAIN_PER_HECTARE ) );
    // ctx.data.servo_vibro_on = 0;
    HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR, parameters_getValue( PARAM_ERROR_MOTOR ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_SERVO, parameters_getValue( PARAM_ERROR_SERVO ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_ERROR_MOTOR_CALIBRATION, parameters_getValue( PARAM_ERROR_MOTOR_CALIBRATION ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_SILOS_HEIGHT_CM, parameters_getValue( PARAM_SILOS_HEIGHT_CM ) );
    HTTPParamClient_SetU32ValueDontWait( PARAM_HIGH_OF_MACHINE_CM, parameters_getValue( PARAM_HIGH_OF_MACHINE_CM ) );
    _change_state( STATE_READY );    // Transition directly to STATE_READY
  }
  else
  {
    menuPrintfInfo( "   Target not connected.\n        Go to DEVICES\n         for connect" );
  }
}

static void _state_ready( void )
{
  if ( !backendIsConnected() )
  {
    _menu_set_error_msg( dictionary_get_string( DICT_LOST_CONNECTION_WITH_SERVER ) );
    return;
  }

  backendEnterMenuAuto();

  if ( _check_low_silos_flag() )
  {
    return;
  }

  if ( ctx.animation_timeout < xTaskGetTickCount() )
  {
    ctx.animation_cnt++;
    ctx.animation_timeout = xTaskGetTickCount() + MS2ST( 100 );
  }

  oled_clearScreen();

  if ( wifiMenu_GetDevType() != T_DEV_TYPE_SIEWNIK )
  {
    oled_printFixed( 2, 3 * LINE_HEIGHT, "Unsupported\ndevice type", OLED_FONT_SIZE_11 );
    return;
  }

  char str[32] = { 0 };

  ssdFigure_DrawLowAccu( 60, 1, parameters_getValue( PARAM_VOLTAGE_ACCUM ), parameters_getValue( PARAM_CURRENT_MOTOR ) );

  if ( parameters_getValue( PARAM_SILOS_SENSOR_IS_CONNECTED ) )
  {
    uint32_t silos_level = parameters_getValue( PARAM_SILOS_LEVEL );
    sprintf( str, "%ld", parameters_getValue( PARAM_SILOS_LEVEL ) );
    if ( silos_level > 99 )
    {
      oled_printFixed( 1, 28, str, OLED_FONT_SIZE_11 );
    }
    else if ( silos_level < 100 && silos_level > 9 )
    {
      oled_printFixed( 3, 28, str, OLED_FONT_SIZE_11 );
    }
    else if ( silos_level < 10 )
    {
      oled_printFixed( 8, 28, str, OLED_FONT_SIZE_11 );
    }
    drawTank( 3, 43, silos_level );
  }

  // Get the GPS status and display icon accordingly
  ctx.velocity_sensor_status = (e108_gnss_status_t) parameters_getValue( PARAM_VELOCITY_SENSOR_STATUS );

  if ( ctx.velocity_sensor_status == E108_READY )
  {
    // Module is working with valid fix - display constantly
    drawGps( 2, 1 );
  }
  else if ( ctx.velocity_sensor_status == E108_WAIT_VALID_MEASUREMENT )
  {
    // Searching for satellites - blink the icon
    if ( ctx.animation_cnt % 2 == 0 )
    {
      drawGps( 2, 1 );
    }
  }

  // Display the center message based on motor status and seeding status
  const char* status_message;

  // If motor is running, show seeding status messages
  if ( ctx.data.is_working )
  {
    if ( parameters_getValue( PARAM_SEEDING_IS_ACTIVE ) )
    {
      status_message = dictionary_get_string( DICT_SEEDING_IN_PROGRESS );    // "Wysiew trwa"
    }
    else
    {
      status_message = dictionary_get_string( DICT_SEEDING_STOPPED );    // "Wysiew zatrzymany"
    }
  }
  else
  {
    // If motor is not running, show GPS status as before
    switch ( ctx.velocity_sensor_status )
    {
      case E108_READY:
        status_message = dictionary_get_string( DICT_READY );    // "Gotowy"
        break;
      case E108_WAIT_VALID_MEASUREMENT:
        status_message = dictionary_get_string( DICT_SEARCHING_FOR_GPS );    // "Szukam GPS"
        break;
      default:    // E108_DISCONNECTED or any other state
        status_message = dictionary_get_string( DICT_SET_SPEED );    // "Ustaw prędkość"
        break;
    }
  }

  bool LargeText = true;    //change large values

  if ( LargeText )
  {
    // Display the status message in the center of the screen
    int text_width = strlen( status_message ) * 6;    // Approximate width based on font size
    int center_x = ( SSD1306_WIDTH - text_width ) / 2;
    oled_printFixed( center_x - 12, 11, status_message, OLED_FONT_SIZE_16 );

    ctx.velocity = (float) parameters_getValue( PARAM_VELOCITY_HMS ) / 10.0f;
    ctx.distance_km = (float) parameters_getValue( PARAM_DISTANCE_HM ) / 10.0f;

    sprintf( str, "%.1f ", ctx.velocity );
    if ( ctx.velocity < 10 )
    {
      oled_printFixed( 28, 32, str, OLED_FONT_SIZE_16 );
    }
    else if ( ctx.velocity < 100 )
    {
      oled_printFixed( 25, 32, str, OLED_FONT_SIZE_16 );
    }
    else
    {
      oled_printFixed( 24, 32, str, OLED_FONT_SIZE_16 );
    }
    drawkm_h( 50, 40 );

    sprintf( str, "%lu ", ctx.data.kg_per_ha );
    if ( ctx.data.kg_per_ha < 10 )
    {
      oled_printFixed( 33, 49, str, OLED_FONT_SIZE_16 );
    }
    else if ( ctx.data.kg_per_ha < 100 )
    {
      oled_printFixed( 29, 49, str, OLED_FONT_SIZE_16 );
    }
    else
    {
      oled_printFixed( 26, 49, str, OLED_FONT_SIZE_16 );
    }
    drawkg_ha( 50, 56 );

    sprintf( str, "%lu ", ctx.data.motor_rpm * 100 );

    if ( ctx.data.motor_rpm < 1 )
    {
      oled_printFixed( 89, 32, str, OLED_FONT_SIZE_16 );
    }
    else if ( ctx.data.motor_rpm < 10 )
    {
      oled_printFixed( 82, 32, str, OLED_FONT_SIZE_16 );
    }
    else if ( ctx.data.motor_rpm < 100 )
    {
      oled_printFixed( 78, 32, str, OLED_FONT_SIZE_16 );
    }
    else
    {
      oled_printFixed( 70, 32, str, OLED_FONT_SIZE_16 );
    }

    drawrpm( 111, 40 );
    sprintf( str, "%.1f ", ctx.distance_km );
    oled_printFixed( 84, 49, str, OLED_FONT_SIZE_16 );
    drawkm( 111, 57 );
  }
  else
  {
    // Display the status message in the center of the screen
    int text_width = strlen( status_message ) * 7;    // Approximate width based on font size
    int center_x = ( SSD1306_WIDTH - text_width ) / 2;
    oled_printFixed( center_x - 5, 11, status_message, OLED_FONT_SIZE_16 );

    ctx.velocity = (float) parameters_getValue( PARAM_VELOCITY_HMS ) / 10.0f;
    ctx.distance_km = (float) parameters_getValue( PARAM_DISTANCE_HM ) / 10.0f;
    sprintf( str, "%.1f ", ctx.velocity );
    if ( ctx.velocity < 10 )
    {
      oled_printFixed( 28, 36, str, OLED_FONT_SIZE_11 );
    }
    else if ( ctx.velocity < 100 )
    {
      oled_printFixed( 25, 36, str, OLED_FONT_SIZE_11 );
    }
    else
    {
      oled_printFixed( 24, 36, str, OLED_FONT_SIZE_11 );
    }
    drawkm_h( 50, 40 );

    sprintf( str, "%lu ", ctx.data.kg_per_ha );
    if ( ctx.data.kg_per_ha < 10 )
    {
      oled_printFixed( 32, 52, str, OLED_FONT_SIZE_11 );
    }
    else if ( ctx.data.kg_per_ha < 100 )
    {
      oled_printFixed( 29, 52, str, OLED_FONT_SIZE_11 );
    }
    else
    {
      oled_printFixed( 26, 52, str, OLED_FONT_SIZE_11 );
    }
    drawkg_ha( 50, 56 );

    sprintf( str, "%lu ", ctx.data.motor_rpm * 100 );

    if ( ctx.data.motor_rpm < 1 )
    {
      oled_printFixed( 89, 36, str, OLED_FONT_SIZE_11 );
    }
    else if ( ctx.data.motor_rpm < 10 )
    {
      oled_printFixed( 82, 36, str, OLED_FONT_SIZE_11 );
    }
    else if ( ctx.data.motor_rpm < 100 )
    {
      oled_printFixed( 78, 36, str, OLED_FONT_SIZE_11 );
    }
    else
    {
      oled_printFixed( 76, 36, str, OLED_FONT_SIZE_11 );
    }

    drawrpm( 109, 40 );
    sprintf( str, "%.1f ", ctx.distance_km );
    oled_printFixed( 84, 52, str, OLED_FONT_SIZE_11 );
    drawkm( 109, 57 );
  }

  //FIX Mariusz  docelowo tutaj bedzie trzeba usunoąć "&& ( ctx.data.is_working == true " jak sie poprawi funkcje wysiewu.
  // Centrala nie widzi ze nie ma modułu."

  // Fix the velocity comparison - check if actual velocity deviates from set velocity by more than 5 km/h
  // Only trigger velocity warnings if GPS has a valid fix
  if ( ( ctx.velocity_sensor_status == E108_READY ) && parameters_getValue( PARAM_SEEDING_IS_ACTIVE ) && ( ctx.velocity < (float) ctx.data.set_velocity - 5 || ctx.velocity > (float) ctx.data.set_velocity + 5 ) )
  {
    if ( !ctx.velocity_warning_triggered )
    {
      ctx.velocity_warning_triggered = true;
      ctx.velocity_warning_timeout = xTaskGetTickCount() + MS2ST( VELOCITY_WARNING_TIMEOUT_MS );
    }
    else if ( ctx.velocity_warning_timeout < xTaskGetTickCount() )
    {
      ctx.velocity_warning_msg_time = xTaskGetTickCount() + MS2ST( 1000 );
      _change_state( STATE_VELOCITY_WARNING );
      ctx.velocity_warning_triggered = false;
    }
  }
  else
  {
    ctx.velocity_warning_triggered = false;
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

  MOTOR_LED_SET_GREEN( 0 );

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
      break;

    case ERROR_VIBRO_OVER_CURRENT:
      oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_VIBRO_OVERCURRENT ), OLED_FONT_SIZE_16 );
      break;

    case ERROR_MOTOR_OVER_CURRENT:
      oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_MOTOR_OVERCURRENT ), OLED_FONT_SIZE_16 );
      motor_led_blink = true;
      break;

    case ERROR_OVER_TEMPERATURE:
      menuPrintfInfo( dictionary_get_string( DICT_TEMPERATURE_IS_HIGH ) );
      motor_led_blink = true;
      break;

    default:
      menuPrintfInfo( dictionary_get_string( DICT_UNKNOWN_ERROR ) );
      motor_led_blink = true;
      break;
  }

  if ( ( blink_counter++ ) % 2 == 0 )
  {
    MOTOR_LED_SET_RED( motor_led_blink ? blink_state : 0 );
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
  oled_printFixed( 20, 8, dictionary_get_string( DICT_VELOCITY ), OLED_FONT_SIZE_26 );
  sprintf( ctx.buff, "%ld  km/h", ctx.data.set_velocity );
  oled_printFixed( CHANGE_VALUE_DISP_OFFSET - 12, MENU_HEIGHT + LINE_HEIGHT + 10, ctx.buff, OLED_FONT_SIZE_26 );    // Font_16x26

  if ( ctx.change_menu_timeout < xTaskGetTickCount() )
  {
    _change_state( STATE_READY );
  }
}

static void _state_kg_per_ha_change( void )
{
  ssdFigure_DrawLowAccu( 60, 1, parameters_getValue( PARAM_VOLTAGE_ACCUM ), parameters_getValue( PARAM_CURRENT_MOTOR ) );

  if ( !backendIsConnected() )
  {
    _menu_set_error_msg( dictionary_get_string( DICT_LOST_CONNECTION_WITH_SERVER ) );
    return;
  }

  ssdFigure_DrawLowAccu( 60, 1, parameters_getValue( PARAM_VOLTAGE_ACCUM ), parameters_getValue( PARAM_CURRENT_MOTOR ) );
  oled_printFixed( 0, 0, "[kg/ha]", OLED_FONT_SIZE_26 );
  sprintf( ctx.buff, "%ld", ctx.data.kg_per_ha );
  oled_printFixed( CHANGE_VALUE_DISP_OFFSET + 5, MENU_HEIGHT + LINE_HEIGHT + 10, ctx.buff, OLED_FONT_SIZE_26 );

  if ( ctx.change_menu_timeout < xTaskGetTickCount() )
  {
    _change_state( STATE_READY );
  }
}

static void _state_motor_change( void )    // Add state handler for STATE_MOTOR_CHANGE
{
  if ( !backendIsConnected() )
  {
    _menu_set_error_msg( dictionary_get_string( DICT_LOST_CONNECTION_WITH_SERVER ) );
    return;
  }
  ssdFigure_DrawLowAccu( 60, 1, parameters_getValue( PARAM_VOLTAGE_ACCUM ), parameters_getValue( PARAM_CURRENT_MOTOR ) );
  oled_printFixed( 0, 0, dictionary_get_string( DICT_MOTOR ), OLED_FONT_SIZE_26 );
  sprintf( ctx.buff, "%ld  rpm", ctx.data.motor_rpm * 100 );    // Changed to motor_rpm
  oled_printFixed( CHANGE_VALUE_DISP_OFFSET - 14, MENU_HEIGHT + LINE_HEIGHT + 5, ctx.buff, OLED_FONT_SIZE_26 );

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
  backendExitMenuAuto();

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

static void _state_velocity_warning( void )
{
  if ( ctx.velocity_warning_msg_time < xTaskGetTickCount() )
  {
    _change_state( STATE_READY );
    return;
  }

  oled_clearScreen();

  if ( (float) ctx.data.set_velocity < ctx.velocity )
  {
    oled_printFixed( 10, 20, dictionary_get_string( DICT_SPEED_DOWN ), OLED_FONT_SIZE_26 );
  }
  else if ( (float) ctx.data.set_velocity > ctx.velocity )
  {
    oled_printFixed( 5, 20, dictionary_get_string( DICT_SPEED_UP ), OLED_FONT_SIZE_26 );
  }
}

static bool menu_process( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
  }

  // Check if both buttons were pressed simultaneously
  if ( ctx.both_buttons_pressed )
  {
    if ( xTaskGetTickCount() < ctx.simultaneous_press_time )
    {
      if ( ctx.button_up_pressed && ctx.button_down_pressed )    // Use stored states instead of Button_GetState
      {
        ctx.both_buttons_pressed = false;
        fastProcessStop( &ctx.data.set_velocity );    // Make sure any fast process is stopped
        fastProcessStop( &ctx.data.kg_per_ha );    // Stop kg_per_ha fast process if active
        enterMenuParameters();
        return true;
      }
    }
    else
    {
      ctx.both_buttons_pressed = false;
    }
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

    case STATE_READY:
      _state_ready();
      break;

    case STATE_ERROR:
      _state_error();
      break;

    case STATE_VELOCITY_CHANGE:
      _state_velocity_change();
      break;

    case STATE_KG_PER_HA_CHANGE:
      _state_kg_per_ha_change();
      break;

    case STATE_MOTOR_CHANGE:    // Add this case
      _state_motor_change();
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

    case STATE_VELOCITY_WARNING:    // Add this case
      _state_velocity_warning();
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
    SERVO_VIBRO_LED_SET_GREEN( parameters_getValue( PARAM_SEEDING_IS_ACTIVE ) );
    MOTOR_LED_SET_RED( 0 );
    SERVO_VIBRO_LED_SET_RED( 0 );
  }

  return true;
}

void menuAutoReset( void )
{
  ctx.data.is_working = false;
  // ctx.data.servo_vibro_on = false;
}

void menuAutoInit( menu_token_t* menu )
{
  memset( &ctx, 0, sizeof( ctx ) );
  menu->menu_cb.enter = menu_enter_cb;
  menu->menu_cb.button_init_cb = menu_button_init_cb;
  menu->menu_cb.exit = menu_exit_cb;
  menu->menu_cb.process = menu_process;
  ctx.button_up_pressed = false;    // Initialize button states
  ctx.button_down_pressed = false;
}

void menuAutoSetError( error_type_t error )
{
  LOG( PRINT_DEBUG, "%s %d", __func__, error );
  ctx.error_dev = error;
  if ( ctx.state == STATE_READY || ctx.state == STATE_VELOCITY_CHANGE || ctx.state == STATE_KG_PER_HA_CHANGE )
  {
    _change_state( STATE_ERROR );
  }
  ctx.data.is_working = false;
  // ctx.data.servo_vibro_on = false;
}

void menuAutoResetError( void )
{
  LOG( PRINT_DEBUG, "%s", __func__ );
  if ( ctx.state == STATE_ERROR )
  {
    ctx.error_dev = ERROR_TOP;
    _change_state( STATE_READY );
  }
}

struct auto_data* menuAutoGetData( void )    // Change return type
{
  return &ctx.data;
}