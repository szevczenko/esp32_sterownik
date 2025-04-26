#include <stdbool.h>

#include "app_config.h"
#include "but.h"
#include "cmd_client.h"
#include "dictionary.h"
#include "freertos/semphr.h"
#include "http_parameters_client.h"
#include "menu_auto.h"
#include "menu_drv.h"
#include "parameters.h"
#include "ssdFigure.h"
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
  STATE_AUTO,
  STATE_EXIT_AUTO,
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
  bool emergency_req;
  bool send_all_data;
  bool auto_mode_sent;
  struct menu_data sended_data;
  struct auto_data sended_auto_data;
  bool menu_auto_is_active;
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
    [STATE_EMERGENCY_DISABLE_EXIT] = "STATE_EMERGENCY_DISABLE_EXIT",
    [STATE_AUTO] = "STATE_AUTO",
    [STATE_EXIT_AUTO] = "STATE_EXIT_AUTO" };

static void change_state( state_backend_t new_state )
{
  if ( ctx.state < STATE_TOP && ctx.state != new_state )
  {
    LOG( PRINT_INFO, "Backend menu %s", state_name[new_state] );
    ctx.state = new_state;
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
    ( HTTPParamClient_SetU32Value( PARAM_EMERGENCY_DISABLE, 1, 2000 ) == ERROR_CODE_OK )
    && ( HTTPParamClient_SetU32Value( PARAM_MOTOR_IS_ON, 0, 2000 ) == ERROR_CODE_OK )
    && ( HTTPParamClient_SetU32Value( PARAM_SERVO_IS_ON, 0, 2000 ) == ERROR_CODE_OK );

  LOG( PRINT_INFO, "%s %d", __func__, ret );
  if ( ret )
  {
    ctx.emergency_msg_sended = true;
    menuStartReset();
  }
}

static void _check_emergency_disable( void )
{
  if ( ctx.emergency_req )
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

  ctx.auto_mode_sent = false;

  osDelay( 50 );
}

static bool _check_error( void )
{
  HTTPParamClient_GetU32Value( PARAM_MACHINE_ERRORS, NULL, 2000 );
  uint32_t errors = parameters_getValue( PARAM_MACHINE_ERRORS );

  if ( errors > 0 )
  {
    for ( uint8_t i = 0; i < ERROR_TOP; i++ )
    {
      if ( errors & ( 1 << i ) )
      {
        menuStartSetError( i );
        menuAutoSetError( i );
      }
    }

    return true;
  }

  return false;
}

static void backend_send_menu_data( void )
{
  struct menu_data* data = menuStartGetData();
  if ( ctx.send_all_data )
  {
    bool result = HTTPParamClient_GetStrValue( PARAM_STR_CONTROLLER_SN, NULL, 0, 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_VIBRO_DUTY_PWM, parameters_getValue( PARAM_VIBRO_DUTY_PWM ), 2000 );
    result &= HTTPParamClient_SetU32Value( PARAM_MOTOR, data->motor_value, 1000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_SERVO, data->servo_value, 1000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_MOTOR_IS_ON, data->motor_on, 1000 );
    result &= HTTPParamClient_SetU32Value( PARAM_SERVO_IS_ON, data->servo_vibro_on, 1000 ) == ERROR_CODE_OK;

    if ( result )
    {
      ctx.send_all_data = false;
      ctx.sended_data.motor_value = data->motor_value;
      ctx.sended_data.servo_value = data->servo_value;
      ctx.sended_data.motor_on = data->motor_on;
      ctx.sended_data.servo_vibro_on = data->servo_vibro_on;
    }
    return;
  }

  if ( data->motor_value != ctx.sended_data.motor_value )
  {
    if ( HTTPParamClient_SetU32Value( PARAM_MOTOR, data->motor_value, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.motor_value = data->motor_value;
    }
  }

  if ( data->servo_value != ctx.sended_data.servo_value )
  {
    if ( HTTPParamClient_SetU32Value( PARAM_SERVO, data->servo_value, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.servo_value = data->servo_value;
    }
  }

  if ( data->motor_on != ctx.sended_data.motor_on )
  {
    if ( HTTPParamClient_SetU32Value( PARAM_MOTOR_IS_ON, data->motor_on, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.motor_on = data->motor_on;
    }
  }

  if ( data->servo_vibro_on != ctx.sended_data.servo_vibro_on )
  {
    if ( HTTPParamClient_SetU32Value( PARAM_SERVO_IS_ON, data->servo_vibro_on, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_data.servo_vibro_on = data->servo_vibro_on;
    }
  }
}

static void backend_send_auto_data( void )
{
  struct auto_data* auto_data = menuAutoGetData();
  if ( ctx.send_all_data )
  {
    bool result = HTTPParamClient_GetStrValue( PARAM_STR_CONTROLLER_SN, NULL, 0, 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_SET_VELOCITY_KM_H, auto_data->set_velocity, 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_GRAIN_PER_HECTARE, auto_data->kg_per_ha, 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_MOTOR_IS_ON, auto_data->is_working, 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_MOTOR_RPM_PER_100, auto_data->motor_rpm, 2000 ) == ERROR_CODE_OK;    // Changed PARAM_MOTOR to PARAM_MOTOR_RPM_PER_100 and motor_value to motor_rpm
    result &= HTTPParamClient_SetU32Value( PARAM_HIGH_OF_MACHINE_CM, parameters_getValue( PARAM_HIGH_OF_MACHINE_CM ), 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_SIZE_OF_GRAIN, parameters_getValue( PARAM_SIZE_OF_GRAIN ), 2000 ) == ERROR_CODE_OK;

    // Add tank geometry parameters
    result &= HTTPParamClient_SetU32Value( PARAM_CUBOID_HEIGHT_CM, parameters_getValue( PARAM_CUBOID_HEIGHT_CM ), 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_BASE_LENGTH_CM, parameters_getValue( PARAM_BASE_LENGTH_CM ), 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_BASE_WIDTH_CM, parameters_getValue( PARAM_BASE_WIDTH_CM ), 2000 ) == ERROR_CODE_OK;

    // Add new auto mode parameters
    result &= HTTPParamClient_SetU32Value( PARAM_WORKING_WIDTH_СM, parameters_getValue( PARAM_WORKING_WIDTH_СM ), 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_CORRECTION_FACTOR, parameters_getValue( PARAM_CORRECTION_FACTOR ), 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_SERVO_OPEN_DELAY_S, parameters_getValue( PARAM_SERVO_OPEN_DELAY_S ), 2000 ) == ERROR_CODE_OK;
    result &= HTTPParamClient_SetU32Value( PARAM_SEEDING_START_SPEED_HMH, parameters_getValue( PARAM_SEEDING_START_SPEED_HMH ), 2000 ) == ERROR_CODE_OK;

    if ( result )
    {
      ctx.send_all_data = false;
      ctx.sended_auto_data.set_velocity = auto_data->set_velocity;
      ctx.sended_auto_data.kg_per_ha = auto_data->kg_per_ha;
      ctx.sended_auto_data.is_working = auto_data->is_working;
      ctx.sended_auto_data.motor_rpm = auto_data->motor_rpm;    // Changed from motor_value to motor_rpm
    }
    return;
  }

  if ( auto_data->set_velocity != ctx.sended_auto_data.set_velocity )
  {
    if ( HTTPParamClient_SetU32Value( PARAM_SET_VELOCITY_KM_H, auto_data->set_velocity, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_auto_data.set_velocity = auto_data->set_velocity;
    }
  }

  if ( auto_data->kg_per_ha != ctx.sended_auto_data.kg_per_ha )
  {
    if ( HTTPParamClient_SetU32Value( PARAM_GRAIN_PER_HECTARE, auto_data->kg_per_ha, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_auto_data.kg_per_ha = auto_data->kg_per_ha;
    }
  }

  if ( auto_data->is_working != ctx.sended_auto_data.is_working )
  {
    if ( HTTPParamClient_SetU32Value( PARAM_MOTOR_IS_ON, auto_data->is_working, 1000 ) == ERROR_CODE_OK )
    {
      ctx.sended_auto_data.is_working = auto_data->is_working;
    }
  }

  if ( auto_data->motor_rpm != ctx.sended_auto_data.motor_rpm )    // Changed from motor_value to motor_rpm
  {
    if ( HTTPParamClient_SetU32Value( PARAM_MOTOR_RPM_PER_100, auto_data->motor_rpm, 1000 ) == ERROR_CODE_OK )    // Changed PARAM_MOTOR to PARAM_MOTOR_RPM_PER_100
    {
      ctx.sended_auto_data.motor_rpm = auto_data->motor_rpm;    // Changed from motor_value to motor_rpm
    }
  }
}

static void backend_start( void )
{
  if ( ctx.get_data_cnt % 5 == 0 )
  {
    bool errors = _check_error() > 0;
    if ( errors )
    {
      LOG( PRINT_INFO, "Error detected on machine" );
    }
    else
    {
      menuStartResetError();
      menuAutoResetError();
      LOG( PRINT_DEBUG, "No error" );
    }

    HTTPParamClient_GetU32Value( PARAM_CURRENT_MOTOR, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_VOLTAGE_ACCUM, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_LOW_LEVEL_SILOS, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_SILOS_LEVEL, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_SILOS_SENSOR_IS_CONNECTED, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_VELOCITY_HMS, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_WORK_AREA, NULL, 2000 );
    LOG( PRINT_DEBUG, "Get silos %d ", parameters_getValue( PARAM_LOW_LEVEL_SILOS ) );
  }

  if ( ctx.get_data_cnt % 20 == 0 )
  {
    ctx.send_all_data = true;
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

  if ( !ctx.auto_mode_sent )
  {
    if ( HTTPParamClient_SetU32Value( PARAM_AUTO_MODE, 0, 1000 ) != ERROR_CODE_OK )
    {
      LOG( PRINT_INFO, "Failed to set PARAM_AUTO_MODE to 0, retrying..." );
      return;
    }
    ctx.auto_mode_sent = true;
  }

  backend_send_menu_data();

  osDelay( 50 );
}

static void backend_auto( void )
{
  if ( ctx.get_data_cnt % 5 == 0 )
  {
    bool errors = _check_error() > 0;
    if ( errors )
    {
      LOG( PRINT_INFO, "Error detected on machine" );
    }
    else
    {
      menuStartResetError();
      menuAutoResetError();
      LOG( PRINT_DEBUG, "No error" );
    }

    HTTPParamClient_GetU32Value( PARAM_CURRENT_MOTOR, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_VOLTAGE_ACCUM, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_LOW_LEVEL_SILOS, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_SILOS_LEVEL, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_SILOS_SENSOR_IS_CONNECTED, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_VELOCITY_HMS, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_WORK_AREA, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_SEEDING_IS_ACTIVE, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_DISTANCE_HM, NULL, 2000 );
    HTTPParamClient_GetU32Value( PARAM_SERVO, NULL, 2000 );
    
    // Add velocity sensor connection status reading
    HTTPParamClient_GetU32Value( PARAM_VELOCITY_SENSOR_STATUS, NULL, 2000 );

    LOG( PRINT_DEBUG, "Get silos %d ", parameters_getValue( PARAM_LOW_LEVEL_SILOS ) );
    LOG( PRINT_DEBUG, "Velocity sensor connected: %d", parameters_getValue( PARAM_VELOCITY_SENSOR_STATUS ) );
  }

  if ( ctx.get_data_cnt % 20 == 0 )
  {
    ctx.send_all_data = true;
  }

  ctx.get_data_cnt++;

  if ( ctx.menu_param_is_active )
  {
    change_state( STATE_MENU_PARAMETERS );
    return;
  }

  if ( !ctx.menu_auto_is_active )
  {
    change_state( STATE_EXIT_AUTO );
    return;
  }

  if ( !ctx.auto_mode_sent )
  {
    if ( HTTPParamClient_SetU32Value( PARAM_AUTO_MODE, 1, 1000 ) != ERROR_CODE_OK )
    {
      LOG( PRINT_INFO, "Failed to set PARAM_AUTO_MODE to 1, retrying..." );
      return;
    }
    ctx.auto_mode_sent = true;
  }

  backend_send_auto_data();

  osDelay( 50 );
}

static void backend_exit_start( void )
{
  change_state( STATE_IDLE );
  backend_send_menu_data();
}

static void backend_exit_auto( void )
{
  backend_send_auto_data();
  change_state( STATE_IDLE );
}

static void backend_menu_parameters( void )
{
  if ( !ctx.menu_param_is_active )
  {
    change_state( STATE_IDLE );
    return;
  }

  HTTPParamClient_GetU32Value( PARAM_TEMPERATURE, NULL, 2000 );
  HTTPParamClient_GetU32Value( PARAM_VOLTAGE_ACCUM, NULL, 2000 );
  HTTPParamClient_GetU32Value( PARAM_CURRENT_MOTOR, NULL, 2000 );
  HTTPParamClient_GetU32Value( PARAM_SILOS_LEVEL, NULL, 2000 );
  osDelay( 50 );
}

static const char* _get_msg( menuDrvMsg_t msg )
{
  switch ( msg )
  {
    case MENU_DRV_MSG_WAIT_TO_INIT:
      return dictionary_get_string( DICT_WAIT_TO_INIT );

    case MENU_DRV_MSG_IDLE_STATE:
      return dictionary_get_string( DICT_MENU_IDLE_STATE );

    case MENU_DRV_MSG_MENU_STOP:
      return dictionary_get_string( DICT_MENU_STOP );

    case MENU_DRV_MSG_POWER_OFF:
      return dictionary_get_string( DICT_POWER_OFF );

    default:
      return NULL;
  }
}

static void backend_error_check( void )
{
  change_state( STATE_IDLE );
}

static void backend_emergency_disable_state( void )
{
  _send_emergency_msg();
  if ( !ctx.emergency_req )
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
    error_code_t ret = HTTPParamClient_SetU32Value( PARAM_EMERGENCY_DISABLE, 0, 2000 );
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

void backendEnterMenuAuto( void )
{
  ctx.menu_auto_is_active = true;
  change_state( STATE_AUTO );
}

void backendExitMenuAuto( void )
{
  ctx.menu_auto_is_active = false;
}

void backendToggleEmergencyDisable( void )
{
  ctx.emergency_req = !ctx.emergency_req && wifiDrvIsConnected();
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

      case STATE_AUTO:
        backend_auto();
        break;

      case STATE_EXIT_AUTO:
        backend_exit_auto();
        break;

      default:
        ctx.state = STATE_IDLE;
        break;
    }
  }
}

void menuBackendInit( void )
{
  menuDrvSetGetMsgCb( _get_msg );
 // menuDrvSetDrawBatteryCb( drawBattery );
  //menuDrvSetDrawSignalCb( drawSignal );
  xTaskCreate( menu_task, "menu_back", 4096, NULL, 5, NULL );
}

bool backendIsConnected( void )
{
  if ( !wifiDrvIsConnected() )
  {
    LOG( PRINT_INFO, "START_MENU: WiFi not connected" );
    return false;
  }

  return true;
}

bool backendIsEmergencyDisable( void )
{
  return ctx.state == STATE_EMERGENCY_DISABLE;
}
