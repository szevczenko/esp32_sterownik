#include "app_config.h"
#include "dictionary.h"
#include "menu_default.h"
#include "menu_drv.h"
#include "power_on.h"
#include "ssd1306.h"
#include "ssdFigure.h"

#define MODULE_NAME "[M_LOW_BAT] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_MENU_BACKEND
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

static bool menu_button_init_cb( void* arg )
{
  menu_token_t* menu = arg;

  if ( menu == NULL )
  {
    NULL_ERROR_MSG();
    return false;
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

  return true;
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
  oled_printFixed( 2, MENU_HEIGHT, dictionary_get_string( DICT_LOW_BATTERY ), OLED_FONT_SIZE_16 );
  oled_printFixed( 2, 2 * MENU_HEIGHT + 5, dictionary_get_string( DICT_CONNECT_CHARGER ), OLED_FONT_SIZE_11 );
  oled_update();

  return true;
}

void menuInitLowBatteryLvl( menu_token_t* menu )
{
  menu->menu_cb.enter = menu_enter_cb;
  menu->menu_cb.button_init_cb = menu_button_init_cb;
  menu->menu_cb.exit = menu_exit_cb;
  menu->menu_cb.process = menu_process;
}
