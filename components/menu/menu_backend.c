#include <stdbool.h>

#include "app_config.h"
#include "but.h"
#include "cmd_client.h"
#include "freertos/semphr.h"
#include "menu_drv.h"
#include "parameters.h"
#include "start_menu.h"
#include "stdarg.h"
#include "stdint.h"
#include "wifidrv.h"

#define MODULE_NAME "[M BACK] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

typedef enum
{
  STATE_INIT,
  STATE_IDLE,
  STATE_START,
  STATE_EXIT_START,
  STATE_MENU_PARAMETERS,
  STATE_ERROR_CHECK,
  STATE_EMERGENCY_DISABLE,
  STATE_EMERGENCY_DISABLE_EXIT,
  STATE_TOP,
} state_backend_t;

typedef struct
{
  state_backend_t state;
  bool error_flag;
  char* error_msg;
  uint32_t get_data_cnt;

  bool menu_start_is_active;
  bool menu_param_is_active;
  bool emergency_msg_sended;
  bool emergency_exit_msg_sended;
  bool emergensy_req;

  bool send_all_data;
  struct menu_data sended_data;
} menu_start_context_t;

static menu_start_context_t ctx;

static char* state_name[] =
  {
    [STATE_INIT] = "STATE_INIT",
    [STATE_IDLE] = "STATE_IDLE",
    [STATE_START] = "STATE_START",
    [STATE_EXIT_START] = "STATE_EXIT_START",
    [STATE_MENU_PARAMETERS] = "STATE_MENU_PARAMETERS",
    [STATE_ERROR_CHECK] = "STATE_ERROR_CHECK",
    [STATE_EMERGENCY_DISABLE] = "STATE_EMERGENCY_DISABLE",
    [STATE_EMERGENCY_DISABLE_EXIT] = "STATE_EMERGENCY_DISABLE_EXIT" };

static void change_state( state_backend_t new_state )
{
  if ( ctx.state < STATE_TOP )
  {
    if ( ctx.state != new_state )
    {
      LOG( PRINT_INFO, "Backend menu %s", state_name[new_state] );
      ctx.state = new_state;
    }
  }
}

static void _enter_emergency( void )
{
  if ( ctx.state != STATE_EMERGENCY_DISABLE )
  {
    LOG( PRINT_INFO, "%s %s", __func__, state_name[ctx.state] );
    change_state( STATE_EMERGENCY_DISABLE );
    ctx.emergency_msg_sended = false;
    ctx.emergency_exit_msg_sended = false;
    menuDrvEnterEmergencyDisable();
  }
}

static void _send_emergency_msg( void )
{
  if ( ctx.emergency_msg_sended )
  {
    return;
  }

  bool ret =
    ( cmdClientSetValue( PARAM_EMERGENCY_DISABLE, 1,
                         2000 )
      == ERROR_CODE_OK )
    && ( cmdClientSetValue( PARAM_MOTOR_IS_ON, 0, 2000 ) == ERROR_CODE_OK ) && ( cmdClientSetValue( PARAM_SERVO_IS_ON, 0, 2000 ) == ERROR_CODE_OK );

  LOG( PRINT_INFO, "%s %d", __func__, ret );
  if ( ret )
  {
    ctx.emergency_msg_sended = true;
    menuStartReset();
  }
}

static void _check_emergency_disable( void )
{
  if ( ctx.emergensy_req )
  {
    _enter_emergency();
  }
}

static void backend_init_state( void )
{
  change_state( STATE_IDLE );
}

static void backend_idle( void )
{
  if ( ctx.menu_param_is_active )
  {
    change_state( STATE_MENU_PARAMETERS );
    return;
  }

  if ( ctx.menu_start_is_active )
  {
    ctx.send_all_data = true;
    change_state( STATE_START );
    return;
  }

  osDelay( 50 );
}

static bool _check_error( void )
{
  cmdClientGetValue( PARAM_MACHINE_ERRORS, NULL, 2000 );
  uint32_t errors = parameters_getValue( PARAM_MACHINE_ERRORS );

  if ( errors > 0 )
  {
    for ( uint8_t i = 0; i < ERROR_TOP; i++ )
    {
      if ( errors & ( 1 << i ) )
      {
        menuStartSetError( i );
      }
    }

    return true;
  }

  return false;
}

static void backend_send_control_data( void )
{
  struct menu_data* data = menuStartGetData();

  if ( ctx.send_all_data )
  {
    if ( cmdClientSetValue( PARAM_MOTOR, data->motor_value, 1000 ) == ERROR_CODE_OK && cmdClientSetValue( PARAM_SERVO, data->servo_value, 1000 ) == ERROR_CODE_OK &&
#if MENU_VIRO_ON_OFF_VERSION
         cmdClientSetValue( PARAM_VIBRO_OFF_S, data->vibro_off_s, 1000 ) == ERROR_CODE_OK && cmdClientSetValue( PARAM_VIBRO_ON_S, data->vibro_on_s, 1000 ) == ERROR_CODE_OK &&
#endif
         cmdClientSetValue( PARAM_MOTOR_IS_ON, data->motor_on, 1000 ) == ERROR_CODE_OK && cmdClientSetValue( PARAM_SERVO_IS_ON, data->servo_vibro_on, 1000 ) == ERROR_CODE_OK )
    {
      ctx.send_all_data = false;
      ctx.sended_data.motor_value = data->motor_value;
      ctx.sended_data.servo_value = data->servo_value;
#if MENU_VIRO_ON_OFF_VERSION
      ctx.sended_data.vibro_off_s = data->vibro_off_s;
      ctx.sended_data.vibro_on_s = data->vibro_on_s;
#endif
      ctx.sended_data.motor_on = data->motor_on;
      ctx.sended_data.servo_vibro_on = data->servo_vibro_on;
    }
  }

  if ( data->motor_value != ctx.sended_data.motor_value )
  {
    if ( cmdClientSetValue( PARAM_MOTOR, data->motor_value, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.motor_value = data->motor_value;
    }
  }

  if ( data->servo_value != ctx.sended_data.servo_value )
  {
    if ( cmdClientSetValue( PARAM_SERVO, data->servo_value, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.servo_value = data->servo_value;
    }
  }
#if MENU_VIRO_ON_OFF_VERSION
  if ( data->vibro_off_s != ctx.sended_data.vibro_off_s )
  {
    if ( cmdClientSetValue( PARAM_VIBRO_OFF_S, data->vibro_off_s, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.vibro_off_s = data->vibro_off_s;
    }
  }

  if ( data->vibro_on_s != ctx.sended_data.vibro_on_s )
  {
    if ( cmdClientSetValue( PARAM_VIBRO_ON_S, data->vibro_on_s, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.vibro_on_s = data->vibro_on_s;
    }
  }
#endif
  if ( data->motor_on != ctx.sended_data.motor_on )
  {
    if ( cmdClientSetValue( PARAM_MOTOR_IS_ON, data->motor_on, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.motor_on = data->motor_on;
    }
  }

  if ( data->servo_vibro_on != ctx.sended_data.servo_vibro_on )
  {
    if ( cmdClientSetValue( PARAM_SERVO_IS_ON, data->servo_vibro_on, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.servo_vibro_on = data->servo_vibro_on;
    }
  }
}

static void backend_start( void )
{
  if ( ctx.get_data_cnt % 20 == 0 )
  {
    bool errors = _check_error() > 0;
    if ( errors )
    {
      LOG( PRINT_INFO, "Error detected on machine" );
    }
    else
    {
      menuStartResetError();
      LOG( PRINT_DEBUG, "No error" );
    }

    cmdClientGetValue( PARAM_CURRENT_MOTOR, NULL, 2000 );
    cmdClientGetValue( PARAM_VOLTAGE_ACCUM, NULL, 2000 );
    cmdClientGetValue( PARAM_LOW_LEVEL_SILOS, NULL, 2000 );
    cmdClientGetValue( PARAM_SILOS_LEVEL, NULL, 2000 );
    cmdClientGetValue( PARAM_SILOS_SENSOR_IS_CONNECTED, NULL, 2000 );
    cmdClientGetString( PARAM_STR_CONTROLLER_SN, NULL, 0, 2000 );
    LOG( PRINT_DEBUG, "Get silos %d ", parameters_getValue( PARAM_LOW_LEVEL_SILOS ) );
  }

  ctx.get_data_cnt++;

  if ( ctx.menu_param_is_active )
  {
    change_state( STATE_MENU_PARAMETERS );
    return;
  }

  if ( !ctx.menu_start_is_active )
  {
    change_state( STATE_EXIT_START );
    return;
  }

  backend_send_control_data();

  osDelay( 10 );
}

static void backend_exit_start( void )
{
  backend_send_control_data();
  change_state( STATE_IDLE );
}

static void backend_menu_parameters( void )
{
  if ( !ctx.menu_param_is_active )
  {
    change_state( STATE_IDLE );
    return;
  }

  cmdClientGetValue( PARAM_TEMPERATURE, NULL, 2000 );
  cmdClientGetValue( PARAM_VOLTAGE_ACCUM, NULL, 2000 );
  cmdClientGetValue( PARAM_CURRENT_MOTOR, NULL, 2000 );
  cmdClientGetValue( PARAM_SILOS_LEVEL, NULL, 2000 );
  osDelay( 50 );
}

static void backend_error_check( void )
{
  change_state( STATE_IDLE );
}

static void backend_emergency_disable_state( void )
{
  _send_emergency_msg();
  if ( !ctx.emergensy_req )
  {
    LOG( PRINT_INFO, "%s exit", __func__ );
    menuDrvExitEmergencyDisable();
    change_state( STATE_EMERGENCY_DISABLE_EXIT );
  }

  osDelay( 50 );
}

static void backend_emergency_disable_exit( void )
{
  if ( !ctx.emergency_exit_msg_sended )
  {
    error_code_t ret = cmdClientSetValue( PARAM_EMERGENCY_DISABLE, 0, 2000 );
    LOG( PRINT_INFO, "%s %d", __func__, ret );
    if ( ret == ERROR_CODE_OK )
    {
      ctx.emergency_exit_msg_sended = true;
    }

    osDelay( 50 );
  }
  else
  {
    change_state( STATE_IDLE );
  }
}

void backendEnterMenuParameters( void )
{
  ctx.menu_param_is_active = true;
}

void backendExitMenuParameters( void )
{
  ctx.menu_param_is_active = false;
}

void backendEnterMenuStart( void )
{
  ctx.menu_start_is_active = true;
}

void backendExitMenuStart( void )
{
  ctx.menu_start_is_active = false;
}

void backendToggleEmergencyDisable( void )
{
  if ( ctx.emergensy_req )
  {
    ctx.emergensy_req = false;
  }
  else
  {
    if ( wifiDrvIsConnected() && cmdClientIsConnected() )
    {
      ctx.emergensy_req = true;
    }
  }
}

static void menu_task( void* arg )
{
  while ( 1 )
  {
    _check_emergency_disable();

    switch ( ctx.state )
    {
      case STATE_INIT:
        backend_init_state();
        break;

      case STATE_IDLE:
        backend_idle();
        break;

      case STATE_START:
        backend_start();
        break;

      case STATE_EXIT_START:
        backend_exit_start();
        break;

      case STATE_MENU_PARAMETERS:
        backend_menu_parameters();
        break;

      case STATE_ERROR_CHECK:
        backend_error_check();
        break;

      case STATE_EMERGENCY_DISABLE:
        backend_emergency_disable_state();
        break;

      case STATE_EMERGENCY_DISABLE_EXIT:
        backend_emergency_disable_exit();
        break;

      default:
        ctx.state = STATE_IDLE;
        break;
    }
  }
}

void menuBackendInit( void )
{
  xTaskCreate( menu_task, "menu_back", 4096, NULL, 5, NULL );
}

bool backendIsConnected( void )
{
  if ( !wifiDrvIsConnected() )
  {
    LOG( PRINT_INFO, "START_MENU: WiFi not connected" );
    return false;
  }

  if ( !cmdClientIsConnected() )
  {
    LOG( PRINT_INFO, "START_MENU: Client not connected" );
    return false;
  }

  return true;
}

bool backendIsEmergencyDisable( void )
{
  return ctx.state == STATE_EMERGENCY_DISABLE;
}
