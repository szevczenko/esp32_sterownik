#include <stdio.h>

#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "xkc-kl200-uart.h"

#define MODULE_NAME "[TANK] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_TANK
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define FILTERED_TABLE_SIZE 10

static uint16_t read_data;
static uint32_t distance;
static uint16_t filtered_tab[FILTERED_TABLE_SIZE];
static uint32_t tab_iterator;
static bool is_connected;
static uint8_t bad_read_data_count;

static void _task( void* arg )
{
  xkc_error_code_t res = xkc_configure_upload_mode( UPLOAD_MODE_MANUAL );
  LOG( PRINT_INFO, "Upload mode: %d\n", res );
  res = xkc_configure_line_mode( LINE_MODE_UART );
  LOG( PRINT_INFO, "Configure line mode: %d\n", res );
  while ( 1 )
  {
    res = xkc_read_distance( &read_data );
    if ( res != XKC_CODE_OK )
    {
      LOG( PRINT_WARNING, "Read distance error: %d\n", res );
    }

    if ( XKC_CODE_OK == res )
    {
      filtered_tab[tab_iterator++ % FILTERED_TABLE_SIZE] = read_data;
      for ( uint32_t j = 0; j < FILTERED_TABLE_SIZE; j++ )
      {
        distance += filtered_tab[j];
      }

      distance = distance / FILTERED_TABLE_SIZE;
      LOG( PRINT_DEBUG, "[SONAR] Read data %d\n\r", read_data );
      is_connected = true;
      bad_read_data_count = 0;
    }
    else
    {
      if ( bad_read_data_count++ > 5 )
      {
        is_connected = false;
      }
    }

    osDelay( 100 );
  }
}

bool tank_sensor_is_connected( void )
{
  return is_connected;
}

uint32_t tank_sensor_get_distance( void )
{
  return distance;
}

void tank_sensor_init( void )
{
  xTaskCreate( _task, "tank_task", 4096, NULL, 10, NULL );
}
