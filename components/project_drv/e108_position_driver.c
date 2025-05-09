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

/**
 * @brief Task states for the GNSS reader state machine
 */
typedef enum
{
  TASK_STATE_INIT, /* Initializing the sensor */
  TASK_STATE_WAIT_ANY_DATA, /* Waiting for initial data from sensor */
  TASK_STATE_WORKING, /* Normal operation - reading data */
  TASK_STATE_DISCONNECT /* Disconnecting/reinitializing due to error */
} e108_task_state_t;

static uint8_t e108_gnss_uart_port = 1;
static gpio_num_t e108_gnss_uart_tx_pin = 19;
static gpio_num_t e108_gnss_uart_rx_pin = 18;
#define E108_GNSS_UART_BAUD 9600
#define E108_GNSS_UART_BAUD_ALT 38400  // Alternative baud rate to try

static uint32_t e108_current_baud_rate = E108_GNSS_UART_BAUD;  // Track current baud rate

#define E108_MAX_NMEA_LENGTH 256
#define E108_TIMEOUT_MS      3000    // Timeout for considering the sensor disconnected

// Kalman filter parameters for speed
#define KALMAN_PROCESS_NOISE     1.0f    // Process noise (Q) - higher values = faster response, more noise
#define KALMAN_MEASUREMENT_NOISE 1.0f    // Measurement noise (R) - higher values = more smoothing
#define KALMAN_ERROR_INIT        1.0f    // Initial error covariance

#define E108_MOVING_AVG_SAMPLES 2    // Size of moving average window
#define E108_MEDIAN_SAMPLES     7    // Size of median filter buffer (odd number recommended)

// EMA filter parameters
#define EMA_ALPHA 0.3f    // Smoothing factor (0-1): higher = more responsive but less smooth

// Alpha-Beta filter parameters
#define ALPHA_PARAM 0.5f    // Position correction factor
#define BETA_PARAM  0.1f    // Velocity correction factor

static const char* TAG = "e108-gnss";

// Global data structure to hold latest position info
typedef struct
{
  double latitude;
  double longitude;
  float altitude;
  float speed_kmh;
  float filtered_speed_kmh;    // Filtered speed using selected filter

  // Kalman filter state variables for speed
  float kalman_gain;    // Kalman gain
  float kalman_estimate_error;    // Estimate error (P)

  // Moving average filter variables
  float speed_history[E108_MOVING_AVG_SAMPLES];    // Array to store speed history
  int speed_history_idx;    // Current index in the circular buffer
  int speed_history_count;    // Number of samples collected (up to E108_MOVING_AVG_SAMPLES)

  // EMA filter variables
  float ema_prev_value;    // Previous EMA filtered value

  // Median filter variables
  float median_buffer[E108_MEDIAN_SAMPLES];    // Buffer for recent speed values
  int median_buffer_idx;    // Current index in circular buffer
  int median_buffer_count;    // Number of samples collected

  // Alpha-Beta filter variables
  float ab_position;    // Current position estimate (speed in km/h)
  float ab_velocity;    // Current velocity estimate (acceleration in km/h/s)
  float ab_last_time_s;    // Time of last update in seconds
  bool ab_initialized;    // Whether the filter has been initialized

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
  e108_gnss_status_t status;    // Current status of the GNSS module
  float distance_km;    // Total distance traveled in kilometers
  uint32_t last_speed_update_ms;    // Timestamp of the last speed update for distance calculation
  e108_filter_t current_filter;    // Currently active filter type
} e108_position_t;

static e108_position_t g_position = { 0 };
static e108_gn03_handle_t g_driver = NULL;
static TaskHandle_t g_continuous_task_handle = NULL;

// Forward declarations
static void e108_continuous_reader_task( void* pvParameters );
static void process_continuous_sentence( const char* sentence );
static bool handle_init_state( void );
static bool handle_wait_any_data_state( char* rx_buffer, size_t buffer_size );
static bool handle_working_state( char* rx_buffer, size_t buffer_size, char* nmea_buffer, int* nmea_index, bool* in_sentence );
static void handle_disconnect_state( void );
static void reset_position_data( void );
static float apply_speed_filter( float new_speed );

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
 * @brief Initialize the GNSS module
 * 
 * @return true if initialization successful, false otherwise
 */
static bool handle_init_state( void )
{
  ESP_LOGI( TAG, "Initializing GNSS module with baud rate %lu", e108_current_baud_rate );

  // Initialize driver if not already done
  if ( g_driver == NULL )
  {
    e108_gn03_config_t config = {
      .uart_port = e108_gnss_uart_port,
      .uart_baud_rate = e108_current_baud_rate,  // Use current baud rate
      .uart_tx_pin = e108_gnss_uart_tx_pin,
      .uart_rx_pin = e108_gnss_uart_rx_pin };

    g_driver = e108_init( &config );
    if ( g_driver == NULL )
    {
      ESP_LOGE( TAG, "Failed to initialize E108-GN03 driver" );
      return false;
    }

    // Configure the module
    if ( E108_OK != e108_set_update_rate( g_driver, 1000 ) )    // 1Hz update rate
    {
      ESP_LOGE( TAG, "Failed to set update rate" );
      e108_deinit( g_driver );
      g_driver = NULL;
      return false;
    }

    if ( E108_OK != e108_set_system_mode( g_driver, E108_MODE_ALL ) )    // Use all satellite systems
    {
      ESP_LOGE( TAG, "Failed to set system mode" );
      e108_deinit( g_driver );
      g_driver = NULL;
      return false;
    }

    if ( E108_OK != setup_nmea_config( g_driver ) )    // Configure NMEA output
    {
      ESP_LOGE( TAG, "Failed to set NMEA output" );
      e108_deinit( g_driver );
      g_driver = NULL;
      return false;
    }

    // Configure UART timeout
    uart_set_rx_timeout( e108_gnss_uart_port, 20 );
  }

  // Reset position data and set initial status
  reset_position_data();

  ESP_LOGI( TAG, "GNSS module initialized successfully" );
  return true;
}

/**
 * @brief Handle the wait-for-data state - wait for any sentence from the sensor
 * 
 * @param rx_buffer Buffer to store raw UART data
 * @param buffer_size Size of the buffer
 * @return true to transition to working state, false to transition to disconnect state
 */
static bool handle_wait_any_data_state( char* rx_buffer, size_t buffer_size )
{
  static bool tried_alternative_baud = false;
  
  // Set timeout for 3 seconds
  uint32_t start_time = ST2MS( xTaskGetTickCount() );
  uint32_t timeout = 3000;    // 3 seconds

  ESP_LOGI( TAG, "Waiting for initial data from GNSS module at %lu baud", e108_current_baud_rate );

  bool valid_data_received = false;
  bool valid_nmea_format = false;

  while ( ST2MS( xTaskGetTickCount() ) - start_time < timeout )
  {
    // Read data from UART with a short timeout
    int len = uart_read_bytes( e108_gnss_uart_port, (uint8_t*) rx_buffer,
                               buffer_size - 1, MS2ST( 100 ) );

    if ( len > 0 )
    {
      rx_buffer[len] = 0;    // Null-terminate
      ESP_LOGI( TAG, "Received data from GNSS module: %s", rx_buffer );
      
      // Check if data contains valid NMEA sentence beginning with '$'
      for (int i = 0; i < len; i++) {
        if (rx_buffer[i] == '$') {
          valid_nmea_format = true;
          break;
        }
      }
      
      if (valid_nmea_format) {
        ESP_LOGI( TAG, "Valid NMEA format detected" );
        valid_data_received = true;
        tried_alternative_baud = false;  // Reset for next time
        break;
      } else {
        ESP_LOGW( TAG, "Received data but not in valid NMEA format" );
      }
    }

    // Small delay to prevent CPU hogging
    vTaskDelay( MS2ST( 10 ) );
  }

  if (!valid_data_received) {
    if (!tried_alternative_baud) {
      ESP_LOGW( TAG, "No valid NMEA data received at %lu baud, trying alternative baud rate", e108_current_baud_rate );
      
      // Deinitialize the driver
      if (g_driver != NULL) {
        e108_deinit( g_driver );
        g_driver = NULL;
      }
      
      // Switch baud rate
      if (e108_current_baud_rate == E108_GNSS_UART_BAUD) {
        e108_current_baud_rate = E108_GNSS_UART_BAUD_ALT;
      } else {
        e108_current_baud_rate = E108_GNSS_UART_BAUD;
      }
      
      tried_alternative_baud = true;
      
      // Return false to go back to init state, but with new baud rate
      return false;
    } else {
      ESP_LOGW( TAG, "No valid data received at either baud rate" );
      tried_alternative_baud = false;  // Reset for next time
      return false;  // Signal to transition to disconnect state
    }
  }

  return valid_data_received;  // Transition to working state if we got valid data
}

/**
 * @brief Handle the working state - read and parse NMEA data
 * 
 * @param rx_buffer Buffer to store raw UART data
 * @param buffer_size Size of the buffer
 * @param nmea_buffer Buffer to store NMEA sentence being constructed
 * @param nmea_index Current index in the NMEA buffer
 * @param in_sentence Flag indicating if we're in the middle of a sentence
 * @return true to stay in working state, false to transition to disconnect state
 */
static bool handle_working_state( char* rx_buffer, size_t buffer_size, char* nmea_buffer, int* nmea_index, bool* in_sentence )
{
  uint32_t current_time = ST2MS( xTaskGetTickCount() );
  bool timeout_occurred = false;

  // Check for timeout
  xSemaphoreTake( g_position.mutex, portMAX_DELAY );
  if ( g_position.last_valid_time_ms > 0 )
  {
    g_position.time_since_last_ms = current_time - g_position.last_valid_time_ms;
    if ( g_position.time_since_last_ms > E108_TIMEOUT_MS )
    {
      timeout_occurred = true;
    }
  }
  xSemaphoreGive( g_position.mutex );

  if ( timeout_occurred )
  {
    ESP_LOGW( TAG, "GNSS module timeout - no data for %d ms", E108_TIMEOUT_MS );
    return false;    // Signal to transition to disconnect state
  }

  // Read data from UART
  int len = uart_read_bytes( e108_gnss_uart_port, (uint8_t*) rx_buffer,
                             buffer_size - 1, MS2ST( 50 ) );

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
        *in_sentence = true;
        *nmea_index = 0;
        nmea_buffer[( *nmea_index )++] = c;
      }
      else if ( *in_sentence )
      {
        // Add character to buffer
        if ( *nmea_index < E108_MAX_NMEA_LENGTH - 1 )
        {
          nmea_buffer[( *nmea_index )++] = c;
        }

        // Check for end of sentence (CR+LF)
        if ( c == '\n' && *nmea_index > 5 )
        {    // Minimum valid sentence length
          nmea_buffer[*nmea_index] = 0;    // Null-terminate

          // Process the sentence immediately
          process_continuous_sentence( nmea_buffer );

          *in_sentence = false;
        }
      }
    }
  }

  return true;    // Stay in working state
}

/**
 * @brief Handle the disconnect state
 */
static void handle_disconnect_state( void )
{
  ESP_LOGW( TAG, "GNSS module disconnected, cleaning up" );

  // Deinitialize driver
  if ( g_driver != NULL )
  {
    e108_deinit( g_driver );
    g_driver = NULL;
  }

  // Update status
  xSemaphoreTake( g_position.mutex, portMAX_DELAY );
  g_position.status = E108_DISCONNECTED;
  xSemaphoreGive( g_position.mutex );

  // Small delay before moving back to init state
  vTaskDelay( MS2ST( 500 ) );
}

/**
 * @brief Reset position data structure
 */
static void reset_position_data( void )
{
  xSemaphoreTake( g_position.mutex, portMAX_DELAY );

  g_position.latitude = 0;
  g_position.longitude = 0;
  g_position.altitude = 0;
  g_position.speed_kmh = 0;
  g_position.filtered_speed_kmh = 0;

  // Initialize Kalman filter
  g_position.kalman_gain = 0.0f;
  g_position.kalman_estimate_error = KALMAN_ERROR_INIT;

  // Initialize moving average filter
  memset( g_position.speed_history, 0, sizeof( g_position.speed_history ) );
  g_position.speed_history_idx = 0;
  g_position.speed_history_count = 0;

  // Initialize EMA filter
  g_position.ema_prev_value = 0.0f;

  // Initialize median filter
  memset( g_position.median_buffer, 0, sizeof( g_position.median_buffer ) );
  g_position.median_buffer_idx = 0;
  g_position.median_buffer_count = 0;

  // Initialize alpha-beta filter
  g_position.ab_position = 0.0f;
  g_position.ab_velocity = 0.0f;
  g_position.ab_last_time_s = 0.0f;
  g_position.ab_initialized = false;

  g_position.course = 0;
  g_position.satellites = 0;
  g_position.fix_quality = 0;
  memset( g_position.timestamp, 0, sizeof( g_position.timestamp ) );
  memset( g_position.datestamp, 0, sizeof( g_position.datestamp ) );
  g_position.valid = false;
  g_position.last_valid_time_ms = 0;
  g_position.time_since_last_ms = 0;
  // Don't reset distance here - we keep total distance across reconnects
  g_position.last_speed_update_ms = 0;

  // Don't reset filter type - it persists across reconnects

  xSemaphoreGive( g_position.mutex );
}

/**
 * @brief Helper function to sort an array of floats for median filter
 */
static void sort_float_array( float arr[], int n )
{
  // Simple bubble sort (adequate for small arrays)
  for ( int i = 0; i < n - 1; i++ )
  {
    for ( int j = 0; j < n - i - 1; j++ )
    {
      if ( arr[j] > arr[j + 1] )
      {
        // Swap elements
        float temp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = temp;
      }
    }
  }
}

/**
 * @brief Apply the currently selected filter to the speed measurement
 * 
 * @param new_speed New speed measurement in km/h
 * @return Filtered speed value in km/h
 */
static float apply_speed_filter( float new_speed )
{
  float filtered_speed = new_speed;    // Default if no filtering applied
  uint32_t current_time_ms = ST2MS( xTaskGetTickCount() );

  switch ( g_position.current_filter )
  {
    case E108_FILTER_NONE:
      // No filtering, just use raw value
      filtered_speed = new_speed;
      break;

    case E108_FILTER_KALMAN:
      // Apply Kalman filter
      if ( g_position.valid )
      {
        // Prediction update - no state update since we assume constant speed between measurements
        // Update error covariance: P = P + Q
        g_position.kalman_estimate_error += KALMAN_PROCESS_NOISE;

        // Measurement update
        // Calculate Kalman gain: K = P / (P + R)
        g_position.kalman_gain = g_position.kalman_estimate_error / ( g_position.kalman_estimate_error + KALMAN_MEASUREMENT_NOISE );

        // Update estimate with measurement: x = x + K * (z - x)
        filtered_speed = g_position.filtered_speed_kmh + g_position.kalman_gain * ( new_speed - g_position.filtered_speed_kmh );

        // Update error covariance: P = (1 - K) * P
        g_position.kalman_estimate_error = ( 1.0f - g_position.kalman_gain ) * g_position.kalman_estimate_error;
      }
      else
      {
        // First valid reading, initialize the filter
        filtered_speed = new_speed;
        g_position.kalman_estimate_error = KALMAN_ERROR_INIT;
        g_position.kalman_gain = 0.0f;
      }
      break;

    case E108_FILTER_MOVING_AVG:
      // Apply moving average filter
      // Store the new value in the circular buffer
      g_position.speed_history[g_position.speed_history_idx] = new_speed;

      // Update index and count
      g_position.speed_history_idx = ( g_position.speed_history_idx + 1 ) % E108_MOVING_AVG_SAMPLES;
      if ( g_position.speed_history_count < E108_MOVING_AVG_SAMPLES )
      {
        g_position.speed_history_count++;
      }

      // Calculate the average of all stored values
      if ( g_position.speed_history_count > 0 )
      {
        float sum = 0.0f;
        for ( int i = 0; i < g_position.speed_history_count; i++ )
        {
          sum += g_position.speed_history[i];
        }
        filtered_speed = sum / g_position.speed_history_count;
      }
      break;

    case E108_FILTER_EMA:
      // Apply Exponential Moving Average filter
      if ( g_position.valid )
      {
        // Apply EMA formula: y(t) = α * x(t) + (1-α) * y(t-1)
        filtered_speed = EMA_ALPHA * new_speed + ( 1.0f - EMA_ALPHA ) * g_position.ema_prev_value;
      }
      else
      {
        // First valid reading, initialize with raw value
        filtered_speed = new_speed;
      }
      // Store current value for next iteration
      g_position.ema_prev_value = filtered_speed;
      break;

    case E108_FILTER_MEDIAN:
      // Apply median filter (excellent for outlier rejection)
      // Add new value to buffer
      g_position.median_buffer[g_position.median_buffer_idx] = new_speed;
      g_position.median_buffer_idx = ( g_position.median_buffer_idx + 1 ) % E108_MEDIAN_SAMPLES;

      // Update count of values in buffer
      if ( g_position.median_buffer_count < E108_MEDIAN_SAMPLES )
      {
        g_position.median_buffer_count++;
      }

      if ( g_position.median_buffer_count > 0 )
      {
        // Create a copy of the buffer for sorting
        float temp_buffer[E108_MEDIAN_SAMPLES];
        memcpy( temp_buffer, g_position.median_buffer,
                g_position.median_buffer_count * sizeof( float ) );

        // Sort the copy
        sort_float_array( temp_buffer, g_position.median_buffer_count );

        // Select middle value as median
        filtered_speed = temp_buffer[g_position.median_buffer_count / 2];
      }
      break;

    case E108_FILTER_ALPHA_BETA:
      // Apply Alpha-Beta filter (simplified tracking filter)
      if ( !g_position.ab_initialized )
      {
        // Initialize filter with current measurement
        g_position.ab_position = new_speed;
        g_position.ab_velocity = 0.0f;
        g_position.ab_last_time_s = current_time_ms / 1000.0f;
        g_position.ab_initialized = true;
        filtered_speed = new_speed;
      }
      else
      {
        // Calculate time delta in seconds
        float current_time_s = current_time_ms / 1000.0f;
        float dt = current_time_s - g_position.ab_last_time_s;

        // Limit dt to reasonable values to prevent instability
        if ( dt > 0.0f && dt < 5.0f )
        {
          // Prediction step
          float predicted_position = g_position.ab_position + g_position.ab_velocity * dt;

          // Calculate residuals (measurement - prediction)
          float residual = new_speed - predicted_position;

          // Correction step
          g_position.ab_position = predicted_position + ALPHA_PARAM * residual;
          g_position.ab_velocity = g_position.ab_velocity + ( BETA_PARAM * residual ) / dt;

          // Update time for next iteration
          g_position.ab_last_time_s = current_time_s;
        }

        filtered_speed = g_position.ab_position;
      }
      break;

    default:
      ESP_LOGW( TAG, "Unknown filter type %d, using raw speed", g_position.current_filter );
      filtered_speed = new_speed;
      break;
  }

  return filtered_speed;
}

/**
 * @brief Continuous GNSS reader task - main state machine
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
  e108_task_state_t state = TASK_STATE_INIT;

  ESP_LOGI( TAG, "Continuous GNSS reader task started" );

  while ( g_position.running )
  {
    switch ( state )
    {
      case TASK_STATE_INIT:
        if ( handle_init_state() )
        {
          ESP_LOGI( TAG, "Transitioning to WAIT_ANY_DATA state" );
          state = TASK_STATE_WAIT_ANY_DATA;
        }
        else
        {
          ESP_LOGW( TAG, "Initialization failed, retrying in 1 second" );
          vTaskDelay( MS2ST( 1000 ) );
        }
        break;

      case TASK_STATE_WAIT_ANY_DATA:
        if ( handle_wait_any_data_state( rx_buffer, sizeof( rx_buffer ) ) )
        {
          ESP_LOGI( TAG, "Transitioning to WORKING state" );
          xSemaphoreTake( g_position.mutex, portMAX_DELAY );
          g_position.status = E108_WAIT_VALID_MEASUREMENT;
          xSemaphoreGive( g_position.mutex );
          state = TASK_STATE_WORKING;
        }
        else
        {
          ESP_LOGW( TAG, "No initial data received, transitioning to DISCONNECT state" );
          state = TASK_STATE_DISCONNECT;
        }
        break;

      case TASK_STATE_WORKING:
        if ( !handle_working_state( rx_buffer, sizeof( rx_buffer ), nmea_buffer, &nmea_index, &in_sentence ) )
        {
          ESP_LOGW( TAG, "Transitioning to DISCONNECT state" );
          state = TASK_STATE_DISCONNECT;
        }
        // Small delay to prevent CPU hogging
        vTaskDelay( MS2ST( 10 ) );
        break;

      case TASK_STATE_DISCONNECT:
        handle_disconnect_state();
        ESP_LOGI( TAG, "Transitioning back to INIT state" );
        state = TASK_STATE_INIT;
        break;
    }
  }

  // Clean up before exit
  if ( g_driver != NULL )
  {
    e108_deinit( g_driver );
    g_driver = NULL;
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

      // Apply the selected filter
      g_position.filtered_speed_kmh = apply_speed_filter( g_position.speed_kmh );

      // Calculate distance based on speed
      if ( g_position.last_speed_update_ms > 0 && g_position.valid && g_position.speed_kmh > 0.5f )
      {
        // Calculate time delta in seconds
        float time_delta_s = ( current_time - g_position.last_speed_update_ms ) / 1000.0f;

        // Only update if time difference is reasonable (prevents large jumps after signal loss)
        if ( time_delta_s > 0 && time_delta_s < 5.0f )
        {
          // Convert km/h to km/s and multiply by seconds
          float distance_delta = ( g_position.filtered_speed_kmh / 3600.0f ) * time_delta_s;

          // Update total distance
          g_position.distance_km += distance_delta;

          ESP_LOGD( TAG, "Added distance: %.6f km (speed: %.2f km/h, time: %.3f s)",
                    distance_delta, g_position.filtered_speed_kmh, time_delta_s );
        }
      }
      g_position.last_speed_update_ms = current_time;

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

    // Update status based on fix quality
    if ( g_position.fix_quality > 0 )
    {
      g_position.status = E108_READY;
    }
    else
    {
      g_position.status = E108_WAIT_VALID_MEASUREMENT;
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

  // Initialize other fields, but let the state machine handle the status
  xSemaphoreTake( g_position.mutex, portMAX_DELAY );
  // The status will be set to E108_DISCONNECTED by the state machine
  g_position.running = true;
  // Initialize distance only if this is the first start (don't reset on reconnect)
  if ( g_position.last_speed_update_ms == 0 )
  {
    g_position.distance_km = 0.0f;
  }
  // Initialize filter type if this is the first start
  if ( g_position.last_speed_update_ms == 0 )
  {
    g_position.current_filter = E108_FILTER_KALMAN;    // Default filter
  }
  g_position.last_speed_update_ms = 0;    // Reset timestamp for distance calculation
  
  // Reset baud rate to default when starting
  e108_current_baud_rate = E108_GNSS_UART_BAUD;
  
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
    xSemaphoreTake( g_position.mutex, portMAX_DELAY );
    g_position.running = false;
    xSemaphoreGive( g_position.mutex );
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

  // Signal the task to stop, but don't modify status - the state machine will do that
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

  // Just update the time since last measurement without modifying status
  uint32_t current_time = ST2MS( xTaskGetTickCount() );
  if ( g_position.last_valid_time_ms > 0 )
  {
    g_position.time_since_last_ms = current_time - g_position.last_valid_time_ms;
  }

  // Copy the data fields without changing status
  position->latitude = g_position.latitude;
  position->longitude = g_position.longitude;
  position->altitude = g_position.altitude;
  position->speed_kmh = g_position.speed_kmh;
  position->filtered_speed_kmh = g_position.filtered_speed_kmh;    // Copy the filtered speed
  position->course = g_position.course;
  position->satellites = g_position.satellites;
  position->fix_quality = g_position.fix_quality;
  strncpy( position->timestamp, g_position.timestamp, sizeof( position->timestamp ) );
  strncpy( position->datestamp, g_position.datestamp, sizeof( position->datestamp ) );
  position->valid = g_position.valid;
  position->time_since_last_ms = g_position.time_since_last_ms;
  position->distance_km = g_position.distance_km;    // Copy distance traveled

  xSemaphoreGive( g_position.mutex );

  return position->valid;
}

/**
 * @brief Get the current status of the GNSS module
 * 
 * @return e108_gnss_status_t Current status of the GNSS module
 */
e108_gnss_status_t e108_get_status( void )
{
  if ( g_position.mutex == NULL )
  {
    return E108_DISCONNECTED;    // Not initialized
  }

  e108_gnss_status_t status;

  // Just read the current status without modifying it
  xSemaphoreTake( g_position.mutex, portMAX_DELAY );

  // Update time since last valid measurement, but don't modify status
  uint32_t current_time = ST2MS( xTaskGetTickCount() );
  if ( g_position.last_valid_time_ms > 0 )
  {
    g_position.time_since_last_ms = current_time - g_position.last_valid_time_ms;
  }

  status = g_position.status;
  xSemaphoreGive( g_position.mutex );

  return status;
}

/**
 * @brief Reset the total distance traveled to zero
 * 
 * @return e108_gnss_err_t E108_GNSS_OK on success, or error code on failure
 */
e108_gnss_err_t e108_reset_distance( void )
{
  if ( g_position.mutex == NULL )
  {
    return E108_GNSS_ERR_INVALID_STATE;
  }

  xSemaphoreTake( g_position.mutex, portMAX_DELAY );
  g_position.distance_km = 0.0f;
  ESP_LOGI( TAG, "Distance reset to 0" );
  xSemaphoreGive( g_position.mutex );

  return E108_GNSS_OK;
}

/**
 * @brief Set the total distance traveled to a specific value
 * 
 * @param initial_distance_km Initial distance in kilometers
 * @return e108_gnss_err_t E108_GNSS_OK on success, or error code on failure
 */
e108_gnss_err_t e108_set_distance( float initial_distance_km )
{
  if ( g_position.mutex == NULL )
  {
    return E108_GNSS_ERR_INVALID_STATE;
  }

  // Check for valid distance value
  if ( initial_distance_km < 0.0f )
  {
    ESP_LOGE( TAG, "Invalid distance value: %.3f km", initial_distance_km );
    return E108_GNSS_ERR_INVALID_ARG;
  }

  xSemaphoreTake( g_position.mutex, portMAX_DELAY );
  g_position.distance_km = initial_distance_km;
  ESP_LOGI( TAG, "Distance set to %.3f km", initial_distance_km );
  xSemaphoreGive( g_position.mutex );

  return E108_GNSS_OK;
}

/**
 * @brief Set the filter type for speed data processing
 * 
 * @param filter_type Type of filter to use
 * @return e108_gnss_err_t E108_GNSS_OK on success, or error code on failure
 */
e108_gnss_err_t e108_set_filter( e108_filter_t filter_type )
{
  if ( g_position.mutex == NULL )
  {
    return E108_GNSS_ERR_INVALID_STATE;
  }

  if ( filter_type >= E108_FILTER_MAX )
  {
    ESP_LOGE( TAG, "Invalid filter type: %d", filter_type );
    return E108_GNSS_ERR_INVALID_ARG;
  }

  xSemaphoreTake( g_position.mutex, portMAX_DELAY );

  // If changing filter type, reset filter state variables
  if ( g_position.current_filter != filter_type )
  {
    // Reset filter-specific state
    switch ( filter_type )
    {
      case E108_FILTER_KALMAN:
        g_position.kalman_estimate_error = KALMAN_ERROR_INIT;
        g_position.kalman_gain = 0.0f;
        break;

      case E108_FILTER_MOVING_AVG:
        memset( g_position.speed_history, 0, sizeof( g_position.speed_history ) );
        g_position.speed_history_idx = 0;
        g_position.speed_history_count = 0;
        break;

      case E108_FILTER_EMA:
        g_position.ema_prev_value = g_position.speed_kmh;
        break;

      case E108_FILTER_MEDIAN:
        memset( g_position.median_buffer, 0, sizeof( g_position.median_buffer ) );
        g_position.median_buffer_idx = 0;
        g_position.median_buffer_count = 0;
        break;

      case E108_FILTER_ALPHA_BETA:
        g_position.ab_position = g_position.speed_kmh;
        g_position.ab_velocity = 0.0f;
        g_position.ab_last_time_s = ST2MS( xTaskGetTickCount() ) / 1000.0f;
        g_position.ab_initialized = true;
        break;

      case E108_FILTER_NONE:
      default:
        // Nothing to initialize for raw data
        break;
    }

    // Set current filter and update filtered speed to match current raw speed
    g_position.current_filter = filter_type;
    g_position.filtered_speed_kmh = g_position.speed_kmh;

    ESP_LOGI( TAG, "Filter changed to %d", filter_type );
  }

  xSemaphoreGive( g_position.mutex );

  return E108_GNSS_OK;
}
