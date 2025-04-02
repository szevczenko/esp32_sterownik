#include "tank_sensor.h"

#include <stdio.h>

#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "parameters.h"
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
#define SILOS_START_MEASURE 100

static uint16_t read_data;
static uint32_t distance;
static uint16_t filtered_tab[FILTERED_TABLE_SIZE];
static uint32_t tab_iterator;
static bool is_connected;
static uint8_t bad_read_data_count;
static int tank_percent;
// Add new variables for volume calculation
static float tank_volume_liters;
static float prev_volume_liters;
static float volume_flow_rate; // liters per minute
static uint32_t last_flow_calc_time;

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

      // Calculate the percentage of silos
      uint32_t tank_height_mm = parameters_getValue( PARAM_SILOS_HEIGHT_CM ) * 10;
      uint32_t tank_distance_mm = distance > SILOS_START_MEASURE ? distance : 0;
      if ( tank_distance_mm > tank_height_mm )
      {
        tank_distance_mm = tank_height_mm;
      }

      // Get tank dimensions from parameters
      uint32_t cuboid_height_mm = parameters_getValue( PARAM_CUBOID_HEIGHT_CM ) * 10;
      uint32_t pyramid_height_mm = tank_height_mm - cuboid_height_mm;
      uint32_t base_length_mm = parameters_getValue( PARAM_BASE_LENGTH_CM ) * 10;
      uint32_t base_width_mm = parameters_getValue( PARAM_BASE_WIDTH_CM ) * 10;
      
      // Calculate liquid height from bottom of tank
      uint32_t liquid_height_mm = tank_height_mm - tank_distance_mm;
      
      // Calculate volume in cubic millimeters
      float volume_mm3 = 0.0f;
      
      if (liquid_height_mm <= pyramid_height_mm) {
        // Liquid level is in pyramid part
        float ratio = (float)liquid_height_mm / pyramid_height_mm;
        volume_mm3 = (1.0f/3.0f) * base_length_mm * base_width_mm * liquid_height_mm * ratio * ratio;
      } else {
        // Liquid level is in cuboid part
        // Add full pyramid volume
        volume_mm3 = (1.0f/3.0f) * base_length_mm * base_width_mm * pyramid_height_mm;
        // Add cuboid volume up to current level
        volume_mm3 += base_length_mm * base_width_mm * (liquid_height_mm - pyramid_height_mm);
      }
      
      // Convert to liters (1 liter = 1,000,000 mm³)
      tank_volume_liters = volume_mm3 / 1000000.0f;
      
      // Calculate flow rate (liters per minute)
      uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
      if (last_flow_calc_time > 0) {
        uint32_t time_diff_ms = current_time - last_flow_calc_time;
        if (time_diff_ms >= 5000) { // Calculate flow every 5 seconds
          float volume_diff = prev_volume_liters - tank_volume_liters;
          // Convert to liters per minute
          volume_flow_rate = (volume_diff * 60000.0f) / time_diff_ms;
          prev_volume_liters = tank_volume_liters;
          last_flow_calc_time = current_time;
        }
      } else {
        // First measurement
        prev_volume_liters = tank_volume_liters;
        last_flow_calc_time = current_time;
      }

      tank_percent = ( tank_height_mm - tank_distance_mm ) * 100 / tank_height_mm;
      if ( ( tank_percent < 0 ) || ( tank_percent > 100 ) )
      {
        tank_percent = 0;
      }

      LOG( PRINT_DEBUG, "Silos height %lu, dist %lu, %d, vol %.2f L, flow %.2f L/min", 
           tank_height_mm, tank_distance_mm, tank_percent, tank_volume_liters, volume_flow_rate );
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

int tank_sensor_get_percent( void )
{
  return tank_percent;
}

// Add new functions to get volume information
float tank_sensor_get_volume(void)
{
  return tank_volume_liters;
}

float tank_sensor_get_flow_rate(void)
{
  return volume_flow_rate;
}

void tank_sensor_init( void )
{
  xTaskCreate( _task, "tank_task", 4096, NULL, 10, NULL );
}
