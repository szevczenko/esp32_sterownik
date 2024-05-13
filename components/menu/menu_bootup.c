#include "app_config.h"
#include "but.h"
#include "cmd_client.h"
#include "dictionary.h"
#include "freertos/semphr.h"
#include "http_parameters_client.h"
#include "menu_backend.h"
#include "menu_default.h"
#include "menu_drv.h"
#include "oled.h"
#include "parameters.h"
#include "parse_cmd.h"
#include "ssdFigure.h"
#include "stdarg.h"
#include "stdint.h"
#include "wifi_menu.h"
#include "wifidrv.h"

#define MODULE_NAME "[BOOTUP] "
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
  STATE_WAIT_WIFI_INIT,
  STATE_CHECK_MEMORY,
  STATE_CONNECT,
  STATE_WAIT_CONNECT,
  STATE_GET_SERVER_DATA,
  STATE_CHECKING_DATA,
  STATE_EXIT,
  STATE_TOP,
} state_bootup_t;

typedef struct
{
  state_bootup_t state;
  bool error_flag;
  const char* error_msg;
  char ap_name[33];
  uint32_t timeout_con;
  bool system_connected;
  bool exit_req;
  char buff[128];
} menu_start_context_t;

static menu_start_context_t ctx;

__attribute__( ( unused ) ) static char* state_name[] =
  {
    [STATE_INIT] = "STATE_INIT",
    [STATE_WAIT_WIFI_INIT] = "STATE_WAIT_WIFI_INIT",
    [STATE_CHECK_MEMORY] = "STATE_CHECK_MEMORY",
    [STATE_CONNECT] = "STATE_CONNECT",
    [STATE_WAIT_CONNECT] = "STATE_WAIT_CONNECT",
    [STATE_GET_SERVER_DATA] = "STATE_GET_SERVER_DATA",
    [STATE_CHECKING_DATA] = "STATE_CHECKING_DATA",
    [STATE_EXIT] = "STATE_EXIT",
};

extern void mainMenuInit( menu_drv_init_t init_type );
extern void enterMenuStart( void );

menu_token_t bootup_menu =
  {
    .name_dict = DICT_LOGO_CLIENT_NAME,
    .arg_type = T_ARG_TYPE_MENU,
};

static void change_state( state_bootup_t new_state )
{
  if ( ctx.state < STATE_TOP )
  {
    if ( ctx.state != new_state )
    {
      LOG( PRINT_INFO, "Bootup menu %s", state_name[new_state] );
    }

    ctx.state = new_state;
  }
  else
  {
    LOG( PRINT_ERROR, "change state %d", new_state );
  }
}

static void bootup_init_state( void )
{
  menuPrintfInfo( dictionary_get_string( DICT_INIT ) );
  change_state( STATE_WAIT_WIFI_INIT );
}

static void bootup_wifi_wait( void )
{
  if ( wifiDrvReadyToConnect() )
  {
    change_state( STATE_CHECK_MEMORY );
  }
  else
  {
    menuPrintfInfo( dictionary_get_string( DICT_WAIT_TO_START_WIFI ) );
  }
}

static void bootup_check_memory( void )
{
  if ( wifiDrvIsReadData() )
  {
    change_state( STATE_CONNECT );
  }
  else
  {
    change_state( STATE_EXIT );
  }
}

static void bootup_connect( void )
{
  wifiDrvGetAPName( ctx.ap_name );
  menuPrintfInfo( "%s %s", dictionary_get_string( DICT_TRY_CONNECT_TO_S ), ctx.ap_name );
  wifiDrvConnect();
  change_state( STATE_WAIT_CONNECT );
}

static void _show_wait_connection( void )
{
  oled_clearScreen();
  sprintf( ctx.buff, dictionary_get_string( DICT_WAIT_CONNECTION_S_S_S ), xTaskGetTickCount() % 400 > 100 ? "." : " ",
           xTaskGetTickCount() % 400 > 200 ? "." : " ", xTaskGetTickCount() % 400 > 300 ? "." : " " );
  oled_printFixed( 2, 2 * MENU_HEIGHT, ctx.buff, OLED_FONT_SIZE_11 );
  oled_update();
}

static void bootup_wait_connect( void )
{
  /* Wait to connect wifi */
  ctx.timeout_con = MS2ST( 5000 ) + xTaskGetTickCount();
  do
  {
    if ( ctx.timeout_con < xTaskGetTickCount() || ctx.exit_req )
    {
      ctx.error_msg = dictionary_get_string( DICT_TIMEOUT_CONNECT );
      ctx.error_flag = 1;
      change_state( STATE_EXIT );
      return;
    }

    _show_wait_connection();
    osDelay( 50 );
  } while ( wifiDrvTryingConnect() );

  ctx.timeout_con = MS2ST( 5000 ) + xTaskGetTickCount();
  do
  {
    if ( ctx.timeout_con < xTaskGetTickCount() || ctx.exit_req )
    {
      ctx.error_msg = dictionary_get_string( DICT_TIMEOUT_SERVER );
      ctx.error_flag = 1;
      change_state( STATE_EXIT );
      return;
    }

    _show_wait_connection();
    osDelay( 50 );
  } while ( !backendIsConnected() );

  oled_clearScreen();
  char ap_name[64] = {};
  wifiDrvGetAPName( ap_name );
  wifiMenu_SetDevType( ap_name );
  menuPrintfInfo( dictionary_get_string( DICT_CONNECTED_TRY_READ_DATA ) );
  change_state( STATE_GET_SERVER_DATA );
}

static void bootup_get_server_data( void )
{
  uint32_t time_to_connect = 0;
  uint32_t start_status = 0;

  while ( HTTPParamClient_GetU32Value( PARAM_START_SYSTEM, &start_status, 150 ) != ERROR_CODE_OK )
  {
    if ( time_to_connect < 5 )
    {
      time_to_connect++;
    }
    else
    {
      LOG( PRINT_INFO, "Timeout get PARAM_START_SYSTEM" );
      change_state( STATE_EXIT );
      return;
    }
  }

  menuPrintfInfo( dictionary_get_string( DICT_READ_DATA_FROM_S ), ctx.ap_name );
  change_state( STATE_CHECKING_DATA );
}

static void bootup_checking_data( void )
{
  ctx.system_connected = true;
  change_state( STATE_EXIT );
  menuPrintfInfo( dictionary_get_string( DICT_SYSTEM_READY_TO_START ) );
}

static void bootup_exit( void )
{
  mainMenuInit( MENU_DRV_NORMAL_INIT );
  if ( ctx.system_connected )
  {
    enterMenuStart();
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

  oled_clearScreen();
  oled_printFixed( 2, 0, dictionary_get_string( DICT_LOGO_CLIENT_NAME ), OLED_FONT_SIZE_16 );
  oled_setGLCDFont( OLED_FONT_SIZE_11 );

  switch ( ctx.state )
  {
    case STATE_INIT:
      bootup_init_state();
      break;

    case STATE_WAIT_WIFI_INIT:
      bootup_wifi_wait();
      break;

    case STATE_CHECK_MEMORY:
      bootup_check_memory();
      break;

    case STATE_CONNECT:
      bootup_connect();
      break;

    case STATE_WAIT_CONNECT:
      bootup_wait_connect();
      break;

    case STATE_GET_SERVER_DATA:
      bootup_get_server_data();
      break;

    case STATE_CHECKING_DATA:
      bootup_checking_data();
      break;

    case STATE_EXIT:
      bootup_exit();
      break;

    default:
      ctx.state = STATE_CONNECT;
      break;
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

  ctx.exit_req = true;

  return true;
}

static void menu_button_exit_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  ctx.exit_req = true;
}

static bool menu_button_init_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
  }

  menu->button.enter.fall_callback = menu_button_exit_callback;
  menu->button.motor_on.fall_callback = menu_button_exit_callback;
  menu->button.exit.fall_callback = menu_button_exit_callback;

  return true;
}

void menuInitBootUpMenu( void )
{
  memset( &ctx, 0, sizeof( ctx ) );
  bootup_menu.menu_cb.enter = menu_enter_cb;
  bootup_menu.menu_cb.button_init_cb = menu_button_init_cb;
  bootup_menu.menu_cb.exit = menu_exit_cb;
  bootup_menu.menu_cb.process = menu_process;

  menuEnter( &bootup_menu );
}
