/**
 * @file e108_test_task.c
 * @brief GNSS module continuous reader based on E108-GN03
 */

#include "e108_position_driver.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "driver/uart.h"
#include "e108-gn03.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static uint8_t e108_gnss_uart_port = 1;
static gpio_num_t e108_gnss_uart_tx_pin = 19;
static gpio_num_t e108_gnss_uart_rx_pin = 18;
#define E108_GNSS_UART_BAUD   9600

#define E108_MAX_NMEA_LENGTH 256

static const char* TAG = "e108-gnss";

// Global data structure to hold latest position info
typedef struct
{
  double latitude;
  double longitude;
  float altitude;
  float speed_kmh;
  float course;
  uint8_t satellites;
  uint8_t fix_quality;
  char timestamp[12];
  char datestamp[8];
  SemaphoreHandle_t mutex;
  bool valid;
  bool running;
  uint32_t last_valid_time_ms;    // Timestamp of last valid measurement in ms
  uint32_t time_since_last_ms;    // Time since last valid measurement in ms
  bool active;    // Sensor is considered active if < 3 sec since last valid data
} e108_position_t;

static e108_position_t g_position = { 0 };
static e108_gn03_handle_t g_driver = NULL;
static TaskHandle_t g_continuous_task_handle = NULL;

// Forward declaration
static void e108_continuous_reader_task( void* pvParameters );
static void process_continuous_sentence( const char* sentence );

/**
 * @brief Convert error code to string for logging
 */
static const char* e108_err_to_str( e108_err_t err )
{
  switch ( err )
  {
    case E108_OK:
      return "OK";
    case E108_ERR_INVALID_ARG:
      return "Invalid argument";
    case E108_ERR_NOT_INITIALIZED:
      return "Not initialized";
    case E108_ERR_UART_CONFIG:
      return "UART config error";
    case E108_ERR_UART_INSTALL:
      return "UART install error";
    case E108_ERR_UART_SET_PIN:
      return "UART set pin error";
    case E108_ERR_UART_WRITE:
      return "UART write error";
    case E108_ERR_UART_READ:
      return "UART read error";
    case E108_ERR_TIMEOUT:
      return "Timeout";
    case E108_ERR_NO_MEMORY:
      return "No memory";
    case E108_ERR_INVALID_RESPONSE:
      return "Invalid response";
    case E108_ERR_CMD_FAILED:
      return "Command failed";
    default:
      return "Unknown error";
  }
}

/**
 * @brief Setup the NMEA sentence output configuration
 */
static e108_err_t setup_nmea_config( e108_gn03_handle_t driver )
{
  // Configure to output GGA, RMC, GSA, and GSV sentences
  e108_nmea_config_t config = {
    .nGGA = 1,    // Output GGA every positioning
    .nGLL = 0,    // Disable GLL
    .nGSA = 1,    // Output GSA every positioning
    .nGSV = 5,    // Output GSV every 5 positioning
    .nRMC = 1,    // Output RMC every positioning
    .nVTG = 1,    // Output VTG every positioning
    .nZDA = 0,    // Disable ZDA
    .nANT = 0,    // Disable ANT
    .nDHV = 0,    // Disable DHV
    .nLPS = 0,    // Disable LPS
    .res1 = -1,    // Reserved
    .res2 = -1,    // Reserved
    .nUTC = 0,    // Disable UTC
    .nGST = 0,    // Disable GST
    .res3 = -1,    // Reserved
    .res4 = -1,    // Reserved
    .res5 = -1,    // Reserved
    .nTIM = 0    // Disable TIM
  };

  return e108_set_nmea_output( driver, &config );
}

/**
 * @brief Continuous GNSS reader task
 * 
 * This task runs in the background and continuously reads and parses
 * NMEA data from the GPS module, updating the global position structure.
 */
static void e108_continuous_reader_task( void* pvParameters )
{
  char rx_buffer[E108_MAX_NMEA_LENGTH];
  char nmea_buffer[E108_MAX_NMEA_LENGTH];
  int nmea_index = 0;
  bool in_sentence = false;

  ESP_LOGI( TAG, "Continuous GNSS reader task started" );

  // Configure a timeout for UART read
  uart_set_rx_timeout( e108_gnss_uart_port, 20 );

  while ( g_position.running )
  {
    int len = uart_read_bytes( e108_gnss_uart_port, (uint8_t*) rx_buffer,
                               sizeof( rx_buffer ) - 1, MS2ST( 50 ) );

    if ( len > 0 )
    {
      rx_buffer[len] = 0;    // Null-terminate

      // Process each character
      for ( int i = 0; i < len; i++ )
      {
        char c = rx_buffer[i];

        if ( c == '$' )
        {
          // Start of a new NMEA sentence
          in_sentence = true;
          nmea_index = 0;
          nmea_buffer[nmea_index++] = c;
        }
        else if ( in_sentence )
        {
          // Add character to buffer
          if ( nmea_index < E108_MAX_NMEA_LENGTH - 1 )
          {
            nmea_buffer[nmea_index++] = c;
          }

          // Check for end of sentence (CR+LF)
          if ( c == '\n' && nmea_index > 5 )
          {    // Minimum valid sentence length
            nmea_buffer[nmea_index] = 0;    // Null-terminate

            // Process the sentence immediately
            process_continuous_sentence( nmea_buffer );

            in_sentence = false;
          }
        }
      }
    }

    // Small delay to prevent CPU hogging
    vTaskDelay( MS2ST( 10 ) );
  }

  ESP_LOGI( TAG, "Continuous GNSS reader task stopped" );
  g_continuous_task_handle = NULL;
  vTaskDelete( NULL );
}

/**
 * @brief Process a received NMEA sentence for continuous reading
 * 
 * Updates the global position structure with data from the NMEA sentence.
 */
static void process_continuous_sentence( const char* sentence )
{
  bool valid_update = false;
  uint32_t current_time = ST2MS( xTaskGetTickCount() );

  // Process RMC sentence (contains most essential navigation data)
  if ( strstr( sentence, "RMC" ) != NULL )
  {
    e108_rmc_data_t rmc_data;

    if ( e108_parse_rmc( sentence, &rmc_data ) == 0 && rmc_data.valid )
    {
      xSemaphoreTake( g_position.mutex, portMAX_DELAY );

      g_position.latitude = rmc_data.latitude;
      g_position.longitude = rmc_data.longitude;
      g_position.speed_kmh = rmc_data.speed * 1.852f;    // Convert knots to km/h
      g_position.course = rmc_data.track_angle;
      strncpy( g_position.timestamp, rmc_data.utc_time, sizeof( g_position.timestamp ) - 1 );
      strncpy( g_position.datestamp, rmc_data.date, sizeof( g_position.datestamp ) - 1 );
      g_position.valid = true;
      valid_update = true;

      xSemaphoreGive( g_position.mutex );
    }
  }
  // Process GGA sentence (contains altitude and fix quality)
  else if ( strstr( sentence, "GGA" ) != NULL )
  {
    e108_gga_data_t gga_data;

    if ( e108_parse_gga( sentence, &gga_data ) == 0 && gga_data.valid )
    {
      xSemaphoreTake( g_position.mutex, portMAX_DELAY );

      g_position.latitude = gga_data.latitude;
      g_position.longitude = gga_data.longitude;
      g_position.altitude = gga_data.altitude;
      g_position.satellites = gga_data.satellites_used;
      g_position.fix_quality = gga_data.fix_quality;
      g_position.valid = true;
      valid_update = true;

      xSemaphoreGive( g_position.mutex );
    }
  }

  // Update timing information if we got valid data
  if ( valid_update )
  {
    xSemaphoreTake( g_position.mutex, portMAX_DELAY );
    g_position.last_valid_time_ms = current_time;
    g_position.time_since_last_ms = 0;
    g_position.active = true;
    xSemaphoreGive( g_position.mutex );
  }
  else
  {
    // Update time since last valid measurement
    xSemaphoreTake( g_position.mutex, portMAX_DELAY );
    if ( g_position.last_valid_time_ms > 0 )
    {
      g_position.time_since_last_ms = current_time - g_position.last_valid_time_ms;
      // Update active status - inactive if more than 3 seconds since last valid measurement
      g_position.active = ( g_position.time_since_last_ms < 3000 );
    }
    xSemaphoreGive( g_position.mutex );
  }
}

/* Helper function to map ESP-IDF errors to our custom error codes */
static e108_gnss_err_t map_esp_err( esp_err_t err )
{
  if ( err == ESP_OK )
    return E108_GNSS_OK;
  else if ( err == ESP_ERR_NO_MEM )
    return E108_GNSS_ERR_NO_MEM;
  else if ( err == ESP_ERR_TIMEOUT )
    return E108_GNSS_ERR_TIMEOUT;
  else if ( err == ESP_ERR_INVALID_ARG )
    return E108_GNSS_ERR_INVALID_ARG;
  else if ( err == ESP_ERR_INVALID_STATE )
    return E108_GNSS_ERR_INVALID_STATE;
  else
    return E108_GNSS_ERR_FAILED;
}

/**
 * @brief Start the continuous GNSS reader
 */
e108_gnss_err_t e108_continuous_start( uint8_t uart_port, uint32_t uart_tx_pin, uint32_t uart_rx_pin )
{
  // Check if already running
  if ( g_continuous_task_handle != NULL )
  {
    return E108_GNSS_ERR_INVALID_STATE;
  }

  // Store the provided pin configuration
  e108_gnss_uart_port = uart_port;
  e108_gnss_uart_tx_pin = uart_tx_pin;
  e108_gnss_uart_rx_pin = uart_rx_pin;
  
  // Initialize the mutex if needed
  if ( g_position.mutex == NULL )
  {
    g_position.mutex = xSemaphoreCreateMutex();
    if ( g_position.mutex == NULL )
    {
      ESP_LOGE( TAG, "Failed to create position mutex" );
      return E108_GNSS_ERR_NO_MEM;
    }
  }

  // Initialize driver if not already done
  if ( g_driver == NULL )
  {
    e108_gn03_config_t config = {
      .uart_port = e108_gnss_uart_port,
      .uart_baud_rate = E108_GNSS_UART_BAUD,
      .uart_tx_pin = e108_gnss_uart_tx_pin,
      .uart_rx_pin = e108_gnss_uart_rx_pin };

    g_driver = e108_init( &config );
    if ( g_driver == NULL )
    {
      ESP_LOGE( TAG, "Failed to initialize E108-GN03 driver" );
      return E108_GNSS_ERR_FAILED;
    }

    // Configure the module
    if ( E108_OK != e108_set_update_rate( g_driver, 1000 ) )    // 1Hz update rate
    {
      ESP_LOGE( TAG, "Failed to set update rate" );
      return E108_GNSS_ERR_FAILED;
    }
    if ( E108_OK != e108_set_system_mode( g_driver, E108_MODE_ALL ) )    // Use all satellite systems
    {
      ESP_LOGE( TAG, "Failed to set system mode" );
      return E108_GNSS_ERR_FAILED;
    }
    if ( E108_OK != setup_nmea_config( g_driver ) )    // Configure NMEA output
    {
      ESP_LOGE( TAG, "Failed to set NMEA output" );
      return E108_GNSS_ERR_FAILED;
    }
  }

  // Reset position data
  xSemaphoreTake( g_position.mutex, portMAX_DELAY );
  g_position.latitude = 0;
  g_position.longitude = 0;
  g_position.altitude = 0;
  g_position.speed_kmh = 0;
  g_position.course = 0;
  g_position.satellites = 0;
  g_position.fix_quality = 0;
  memset( g_position.timestamp, 0, sizeof( g_position.timestamp ) );
  memset( g_position.datestamp, 0, sizeof( g_position.datestamp ) );
  g_position.valid = false;
  g_position.running = false;
  g_position.mutex = g_position.mutex;    // Preserve the mutex
  g_position.running = true;
  g_position.last_valid_time_ms = 0;
  g_position.time_since_last_ms = 0;
  g_position.active = false;
  xSemaphoreGive( g_position.mutex );

  // Start the continuous reader task
  BaseType_t res = xTaskCreate(
    e108_continuous_reader_task,
    "e108_gnss",
    4096,
    NULL,
    5,
    &g_continuous_task_handle );

  if ( res != pdPASS )
  {
    ESP_LOGE( TAG, "Failed to create continuous GNSS reader task" );
    g_position.running = false;
    return E108_GNSS_ERR_FAILED;
  }

  ESP_LOGI( TAG, "Continuous GNSS reader started" );
  return E108_GNSS_OK;
}

/**
 * @brief Stop the continuous GNSS reader
 */
e108_gnss_err_t e108_continuous_stop( void )
{
  if ( g_continuous_task_handle == NULL )
  {
    return E108_GNSS_ERR_INVALID_STATE;
  }

  // Signal the task to stop
  xSemaphoreTake( g_position.mutex, portMAX_DELAY );
  g_position.running = false;
  xSemaphoreGive( g_position.mutex );

  // Wait for task to finish (with timeout)
  uint32_t timeout = 0;
  while ( g_continuous_task_handle != NULL && timeout < 20 )
  {
    vTaskDelay( MS2ST( 100 ) );
    timeout++;
  }

  // Deinitialize driver if needed
  if ( g_driver != NULL )
  {
    e108_deinit( g_driver );
    g_driver = NULL;
  }

  ESP_LOGI( TAG, "Continuous GNSS reader stopped" );
  return E108_GNSS_OK;
}

/**
 * @brief Get the latest position data
 * 
 * @param position Pointer to structure to store position data
 * @return bool True if the position is valid, false otherwise
 */
bool e108_get_position( e108_position_info_t* position )
{
  if ( position == NULL || g_position.mutex == NULL )
  {
    return false;
  }

  xSemaphoreTake( g_position.mutex, portMAX_DELAY );

  // Update time since last valid measurement and active status
  uint32_t current_time = ST2MS( xTaskGetTickCount() );
  if ( g_position.last_valid_time_ms > 0 )
  {
    g_position.time_since_last_ms = current_time - g_position.last_valid_time_ms;
    g_position.active = ( g_position.time_since_last_ms < 3000 );    // 3 seconds threshold
  }

  // Copy only the data fields
  position->latitude = g_position.latitude;
  position->longitude = g_position.longitude;
  position->altitude = g_position.altitude;
  position->speed_kmh = g_position.speed_kmh;
  position->course = g_position.course;
  position->satellites = g_position.satellites;
  position->fix_quality = g_position.fix_quality;
  strncpy( position->timestamp, g_position.timestamp, sizeof( position->timestamp ) );
  strncpy( position->datestamp, g_position.datestamp, sizeof( position->datestamp ) );
  position->valid = g_position.valid;
  position->time_since_last_ms = g_position.time_since_last_ms;

  xSemaphoreGive( g_position.mutex );

  return position->valid;
}

/**
 * @brief Check if the GNSS module is currently active
 * 
 * @return bool True if the GNSS has received valid data in the last 3 seconds
 */
bool e108_is_active( void )
{
  if ( g_position.mutex == NULL )
  {
    return false;    // Not initialized
  }

  bool active = false;

  // Update active status before returning
  xSemaphoreTake( g_position.mutex, portMAX_DELAY );

  uint32_t current_time = ST2MS( xTaskGetTickCount() );
  if ( g_position.last_valid_time_ms > 0 )
  {
    g_position.time_since_last_ms = current_time - g_position.last_valid_time_ms;
    g_position.active = ( g_position.time_since_last_ms < 3000 );    // 3 seconds threshold
  }

  active = g_position.active;
  xSemaphoreGive( g_position.mutex );

  return active;
}
