#include "app_config.h"
#include "cmd_client.h"
#include "menu_default.h"
#include "menu_drv.h"
#include "oled.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "wifidrv.h"

#define MODULE_NAME "[SETTING] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define DEVICE_LIST_SIZE 32

typedef enum
{
  ST_WIFI_INIT,
  ST_WIFI_IDLE,
  ST_WIFI_FIND_DEVICE,
  ST_WIFI_DEVICE_LIST,
  ST_WIFI_DEVICE_TRY_CONNECT,
  ST_WIFI_DEVICE_WAIT_CONNECT,
  ST_WIFI_DEVICE_WAIT_CMD_CLIENT,
  ST_WIFI_CONNECTED,
  ST_WIFI_ERROR_CHECK,
  ST_WIFI_STOP,
} stateWifiMenu_t;

typedef struct
{
  stateWifiMenu_t state;
  uint16_t ap_count;
  uint16_t devices_count;
  uint32_t timeout_con;
  char devices_list[DEVICE_LIST_SIZE][33];
  bool connect_req;
  bool exit_req;
  bool scan_req;
  bool error_flag;
  int error_code;
  const char* error_msg;
} wifi_menu_t;

static wifi_menu_t ctx;
static uint8_t dev_type;

static char* state_name[] =
  {
    [ST_WIFI_INIT] = "ST_WIFI_INIT",
    [ST_WIFI_IDLE] = "ST_WIFI_IDLE",
    [ST_WIFI_FIND_DEVICE] = "ST_WIFI_FIND_DEVICE",
    [ST_WIFI_DEVICE_LIST] = "ST_WIFI_DEVICE_LIST",
    [ST_WIFI_DEVICE_TRY_CONNECT] = "ST_WIFI_DEVICE_TRY_CONNECT",
    [ST_WIFI_DEVICE_WAIT_CONNECT] = "ST_WIFI_DEVICE_WAIT_CONNECT",
    [ST_WIFI_DEVICE_WAIT_CMD_CLIENT] = "ST_WIFI_DEVICE_WAIT_CMD_CLIENT",
    [ST_WIFI_CONNECTED] = "ST_WIFI_CONNECTED",
    [ST_WIFI_ERROR_CHECK] = "ST_WIFI_ERROR_CHECK",
    [ST_WIFI_STOP] = "ST_WIFI_STOP" };

static scrollBar_t scrollBar =
  {
    .line_max = MAX_LINE,
    .y_start = MENU_HEIGHT };

static void change_state( stateWifiMenu_t new_state )
{
  ctx.state = new_state;
  LOG( PRINT_INFO, "WiFi menu %s", state_name[new_state] );
}

static void menu_button_up_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  if ( ctx.state != ST_WIFI_DEVICE_LIST )
  {
    return;
  }

  menu->last_button = LAST_BUTTON_UP;
  if ( menu->position > 0 )
  {
    menu->position--;
  }
}

static void menu_button_down_callback( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return;
  }

  if ( ctx.state != ST_WIFI_DEVICE_LIST )
  {
    return;
  }

  menu->last_button = LAST_BUTTON_DOWN;
  if ( menu->position < ctx.devices_count - 1 )
  {
    menu->position++;
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

  if ( ( ctx.state == ST_WIFI_IDLE ) || ( ctx.state == ST_WIFI_ERROR_CHECK ) )
  {
    ctx.scan_req = true;
  }
  else if ( ctx.state == ST_WIFI_DEVICE_LIST )
  {
    if ( ctx.devices_count == 0 )
    {
      ctx.scan_req = true;
    }
    else
    {
      ctx.connect_req = true;
    }
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

  ctx.scan_req = true;
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

static bool connectToDevice( char* dev )
{
  LOG( PRINT_INFO, "Try connect %s ", dev );

  /* Check device type */
  dev_type = 0xFF;
  if ( memcmp( WIFI_SIEWNIK_NAME, dev, strlen( WIFI_SIEWNIK_NAME ) - 1 ) == 0 )
  {
    dev_type = T_DEV_TYPE_SIEWNIK;
  }
  else if ( memcmp( WIFI_SOLARKA_NAME, dev, strlen( WIFI_SOLARKA_NAME ) - 1 ) == 0 )
  {
    dev_type = T_DEV_TYPE_SOLARKA;
  }
  else if ( memcmp( WIFI_VALVE_NAME, dev, strlen( WIFI_VALVE_NAME ) - 1 ) == 0 )
  {
    dev_type = T_DEV_TYPE_VALVE;
  }

  if ( dev_type != 0xFF )
  {
    /* Disconnect if connected */
    if ( wifiDrvIsConnected() )
    {
      if ( wifiDrvDisconnect() != ESP_OK )
      {
        ctx.error_msg = "conToDevice err";
        return false;
      }
    }

    wifiDrvSetAPName( dev, strlen( dev ) + 1 );
    wifiDrvSetPassword( WIFI_AP_PASSWORD, strlen( WIFI_AP_PASSWORD ) );

    /* Wait to wifi drv ready connect */
    uint32_t wait_to_ready = MS2ST( 1000 ) + xTaskGetTickCount();
    do
    {
      if ( wait_to_ready < xTaskGetTickCount() )
      {
        ctx.error_msg = "Wifi drv not ready";
        return false;
      }

      osDelay( 50 );
    } while ( !wifiDrvReadyToConnect() );

    if ( wifiDrvConnect() != true )
    {
      ctx.error_msg = "conToDevice err";
      return false;
    }

    return true;
  }
  else
  {
    dev_type = T_DEV_TYPE_VALVE;
  }

  ctx.error_msg = "Bad dev name";
  return false;
}

static void menu_wifi_init( void )
{
  if ( wifiDrvIsReadyToScan() )
  {
    change_state( ST_WIFI_IDLE );
  }

  oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_WAIT_TO_WIFI_INIT ), OLED_FONT_SIZE_11 );
}

static void menu_wifi_idle( void )
{
  if ( ctx.scan_req )
  {
    oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_SCANNING_DEVICES ), OLED_FONT_SIZE_11 );
    change_state( ST_WIFI_FIND_DEVICE );
    return;
  }

  oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_CLICK_ENTER_TO_SCANNING ), OLED_FONT_SIZE_11 );
}

static void menu_wifi_find_devices( void )
{
  static char dev_name[33] = { 0 };

  int err = wifiDrvStartScan();

  if ( err == ESP_OK )
  {
    ctx.devices_count = 0;
    wifiDrvGetScanResult( &ctx.ap_count );
    for ( uint16_t i = 0; i < ctx.ap_count; i++ )
    {
      if ( ctx.ap_count > DEVICE_LIST_SIZE )
      {
        break;
      }

      wifiDrvGetNameFromScannedList( i, dev_name );
      if ( ( memcmp( WIFI_SIEWNIK_NAME, dev_name, strlen( WIFI_SIEWNIK_NAME ) - 1 ) == 0 )
           || ( memcmp( WIFI_SOLARKA_NAME, dev_name, strlen( WIFI_SOLARKA_NAME ) - 1 ) == 0 )
           || ( memcmp( WIFI_VALVE_NAME, dev_name, strlen( WIFI_VALVE_NAME ) - 1 ) == 0 ) )
      {
        LOG( PRINT_INFO, "%s\n", dev_name );
        strncpy( ctx.devices_list[ctx.devices_count++], dev_name, 33 );
      }
    }

    change_state( ST_WIFI_DEVICE_LIST );
  }
  else
  {
    change_state( ST_WIFI_ERROR_CHECK );
    ctx.error_flag = true;
    ctx.error_code = err;
    ctx.error_msg = "WiFi Scan Error";
  }

  ctx.scan_req = false;
}

static void menu_wifi_show_list( menu_token_t* menu )
{
  static char buff[128];

  if ( ctx.devices_count == 0 )
  {
    if ( ctx.scan_req )
    {
      oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_FIND_DEVICES ), OLED_FONT_SIZE_11 );
      change_state( ST_WIFI_FIND_DEVICE );
      return;
    }

    oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_DEVICE_NOT_FOUND ), OLED_FONT_SIZE_11 );
    return;
  }

  if ( ctx.connect_req )
  {
    ctx.connect_req = false;
    sprintf( buff, "%s\n%s", dictionary_get_string( DICT_TRY_CONNECT_TO ), ctx.devices_list[menu->position] );
    oled_printFixed( 2, MENU_HEIGHT, buff, OLED_FONT_SIZE_11 );
    change_state( ST_WIFI_DEVICE_TRY_CONNECT );
    return;
  }

  if ( menu->line.end - menu->line.start != MAX_LINE - 1 )
  {
    menu->line.start = menu->position;
    menu->line.end = menu->line.start + MAX_LINE - 1;
  }

  if ( ( menu->position < menu->line.start ) || ( menu->position > menu->line.end ) )
  {
    if ( menu->last_button )
    {
      menu->line.start = menu->position;
      menu->line.end = menu->line.start + MAX_LINE - 1;
    }
    else
    {
      menu->line.end = menu->position;
      menu->line.start = menu->line.end - MAX_LINE + 1;
    }
  }

  //LOG(PRINT_INFO, "position %d, ctx.devices_count %d menu->line.start %d\n", menu->position, ctx.devices_count, menu->line.start);
  int line = 0;

  do
  {
    if ( line + menu->line.start == menu->position )
    {
      ssdFigureFillLine( MENU_HEIGHT + LINE_HEIGHT * line, LINE_HEIGHT );
      oled_printFixedBlack( 2, MENU_HEIGHT + LINE_HEIGHT * line, &ctx.devices_list[line + menu->line.start][strlen( WIFI_SOLARKA_NAME ) + 1], OLED_FONT_SIZE_11 );
    }
    else
    {
      oled_printFixed( 2, MENU_HEIGHT + LINE_HEIGHT * line, &ctx.devices_list[line + menu->line.start][strlen( WIFI_SOLARKA_NAME ) + 1], OLED_FONT_SIZE_11 );
    }

    line++;
  } while ( line + menu->line.start < ctx.devices_count && line < MAX_LINE );

  scrollBar.actual_line = menu->position;
  scrollBar.all_line = ctx.devices_count - 1;
  ssdFigureDrawScrollBar( &scrollBar );
}

static void menu_wifi_connect( menu_token_t* menu )
{
  if ( connectToDevice( ctx.devices_list[menu->position] ) )
  {
    oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_WAIT_TO_CONNECT ), OLED_FONT_SIZE_11 );
    oled_printFixed( 2, MENU_HEIGHT + LINE_HEIGHT, ctx.devices_list[menu->position], OLED_FONT_SIZE_11 );
    change_state( ST_WIFI_DEVICE_WAIT_CONNECT );
  }
  else
  {
    ctx.error_flag = 1;
    ctx.error_msg = "Error connected";
    change_state( ST_WIFI_ERROR_CHECK );
  }
}

static void menu_wifi_wait_connect( void )
{
  /* Wait to connect wifi */
  ctx.timeout_con = MS2ST( 10000 ) + xTaskGetTickCount();
  do
  {
    if ( ctx.timeout_con < xTaskGetTickCount() )
    {
      ctx.error_msg = dictionary_get_string( DICT_TIMEOUT_CONNECT );
      ctx.error_flag = 1;
      change_state( ST_WIFI_ERROR_CHECK );
      return;
    }

    osDelay( 50 );
  } while ( wifiDrvTryingConnect() );

  oled_clearScreen();

  if ( wifiDrvIsConnected() )
  {
    oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_WIFI_CONNECTED ), OLED_FONT_SIZE_11 );
    oled_printFixed( 2, MENU_HEIGHT + LINE_HEIGHT, dictionary_get_string( DICT_WAIT_TO_SERVER ), OLED_FONT_SIZE_11 );
    change_state( ST_WIFI_DEVICE_WAIT_CMD_CLIENT );
  }
  else
  {
    ctx.error_msg = "Error connect";
    ctx.error_flag = 1;
    change_state( ST_WIFI_ERROR_CHECK );
  }
}

static void menu_wifi_wait_cmd_client( void )
{
  /* Wait to cmd server */
  ctx.timeout_con = MS2ST( 10000 ) + xTaskGetTickCount();
  do
  {
    if ( ctx.timeout_con < xTaskGetTickCount() )
    {
      ctx.error_msg = dictionary_get_string( DICT_TIMEOUT_SERVER );
      ctx.error_flag = 1;
      change_state( ST_WIFI_ERROR_CHECK );
      return;
    }

    osDelay( 50 );
  } while ( !cmdClientIsConnected() );

  change_state( ST_WIFI_CONNECTED );
}

static void menu_wifi_error_check( void )
{
  static char error_buff[64];

  if ( ctx.error_flag )
  {
    oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_ERROR_CONNECT ), OLED_FONT_SIZE_11 );
    if ( ctx.error_msg != NULL )
    {
      sprintf( error_buff, "%s:%d", ctx.error_msg, ctx.error_code );
      LOG( PRINT_INFO, "Wifi error [%d] %s", ctx.error_code, ctx.error_msg );
    }

    wifiDrvDisconnect();
    if ( ctx.scan_req )
    {
      change_state( ST_WIFI_STOP );
    }
    else
    {
      return;
    }
  }
  else
  {
    change_state( ST_WIFI_IDLE );
  }

  ctx.error_flag = false;
  ctx.error_msg = NULL;
  ctx.error_code = 0;
}

static void menu_wifi_connected( menu_token_t* menu )
{
  oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_CONNECTED_TO ), OLED_FONT_SIZE_11 );
  oled_printFixed( 2, MENU_HEIGHT + LINE_HEIGHT, ctx.devices_list[menu->position], OLED_FONT_SIZE_11 );

  if ( ctx.scan_req )
  {
    change_state( ST_WIFI_STOP );
  }
  else
  {
    osDelay( 1000 );
    menuDrv_Exit( menu );
    enterMenuStart();
  }
}

static void menu_wifi_stop( void )
{
  wifiDrvDisconnect();
  change_state( ST_WIFI_INIT );
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
  oled_printFixed( 2, 0, dictionary_get_string( menu->name_dict ), OLED_FONT_SIZE_16 );

  switch ( ctx.state )
  {
    case ST_WIFI_INIT:
      menu_wifi_init();
      break;

    case ST_WIFI_IDLE:
      menu_wifi_idle();
      break;

    case ST_WIFI_FIND_DEVICE:
      menu_wifi_find_devices();
      break;

    case ST_WIFI_DEVICE_LIST:
      menu_wifi_show_list( menu );
      break;

    case ST_WIFI_DEVICE_TRY_CONNECT:
      menu_wifi_connect( menu );
      break;

    case ST_WIFI_DEVICE_WAIT_CONNECT:
      menu_wifi_wait_connect();
      break;

    case ST_WIFI_DEVICE_WAIT_CMD_CLIENT:
      menu_wifi_wait_cmd_client();
      break;

    case ST_WIFI_CONNECTED:
      menu_wifi_connected( menu );
      break;

    case ST_WIFI_STOP:
      menu_wifi_stop();
      break;

    case ST_WIFI_ERROR_CHECK:
      menu_wifi_error_check();
      break;

    default:
      change_state( ST_WIFI_STOP );
      break;
  }

  return true;
}

void menuInitWifiMenu( menu_token_t* menu )
{
  memset( &ctx, 0, sizeof( ctx ) );
  menu->menu_cb.enter = menu_enter_cb;
  menu->menu_cb.button_init_cb = menu_button_init_cb;
  menu->menu_cb.exit = menu_exit_cb;
  menu->menu_cb.process = menu_process;
}

uint8_t wifiMenu_GetDevType( void )
{
  return dev_type;
}
