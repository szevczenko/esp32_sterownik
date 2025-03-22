/**
 * @file e108_test_task.h
 * @brief Header for E108-GN03 GNSS module continuous reader
 */

#ifndef E108_TEST_TASK_H
#define E108_TEST_TASK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error codes for the E108 GNSS reader
 */
typedef enum
{
  E108_GNSS_OK = 0, /* Operation completed successfully */
  E108_GNSS_ERR_INVALID_ARG = -1, /* Invalid argument supplied */
  E108_GNSS_ERR_NO_MEM = -2, /* Memory allocation failed */
  E108_GNSS_ERR_INVALID_STATE = -3, /* Driver in invalid state */
  E108_GNSS_ERR_FAILED = -4, /* Generic operation failure */
  E108_GNSS_ERR_TIMEOUT = -5 /* Operation timed out */
} e108_gnss_err_t;

/**
 * @brief Status codes for the E108 GNSS module
 */
typedef enum
{
  E108_DISCONNECTED = 0, /* No communication with GNSS module */
  E108_WAIT_VALID_MEASUREMENT, /* Data received but no valid fix yet */
  E108_READY /* Valid position data available */
} e108_gnss_status_t;

/**
 * @brief Structure to hold latest position data
 */
typedef struct
{
  double latitude; /* Latitude in decimal degrees */
  double longitude; /* Longitude in decimal degrees */
  float altitude; /* Altitude in meters */
  float speed_kmh; /* Speed in kilometers per hour */
  float filtered_speed_kmh; /* Filtered speed in kilometers per hour */
  float course; /* Course over ground in degrees */
  uint8_t satellites; /* Number of satellites used for fix */
  uint8_t fix_quality; /* Fix quality indicator */
  char timestamp[12]; /* UTC time in hhmmss.sss format */
  char datestamp[8]; /* Date in ddmmyy format */
  bool valid; /* True if the position is valid */
  uint32_t time_since_last_ms;    // Time since last valid measurement in ms
  float distance_km;              /* Total distance traveled in kilometers */
} e108_position_info_t;

/**
 * @brief Start the continuous GNSS reader
 * 
 * This function starts a background task that continuously reads
 * NMEA data from the GNSS module and updates the position information.
 * 
 * @param uart_port UART port number to use for communication
 * @param uart_tx_pin TX pin for UART communication
 * @param uart_rx_pin RX pin for UART communication
 * @return e108_gnss_err_t E108_GNSS_OK on success, or error code on failure
 */
e108_gnss_err_t e108_continuous_start( uint8_t uart_port, uint32_t uart_tx_pin, uint32_t uart_rx_pin );

/**
 * @brief Stop the continuous GNSS reader
 * 
 * @return e108_gnss_err_t E108_GNSS_OK on success, or error code on failure
 */
e108_gnss_err_t e108_continuous_stop( void );

/**
 * @brief Get the latest position data
 * 
 * @param position Pointer to structure to store position data
 * @return bool True if the position is valid, false otherwise
 */
bool e108_get_position( e108_position_info_t* position );

/**
 * @brief Get the current status of the GNSS module
 * 
 * @return e108_gnss_status_t Current status of the GNSS module
 */
e108_gnss_status_t e108_get_status( void );

/**
 * @brief Reset the total distance traveled to zero
 * 
 * @return e108_gnss_err_t E108_GNSS_OK on success, or error code on failure
 */
e108_gnss_err_t e108_reset_distance( void );

/**
 * @brief Set the total distance traveled to a specific value
 * 
 * This can be used to restore a previously saved distance value
 * 
 * @param initial_distance_km Initial distance in kilometers
 * @return e108_gnss_err_t E108_GNSS_OK on success, or error code on failure
 */
e108_gnss_err_t e108_set_distance( float initial_distance_km );

#ifdef __cplusplus
}
#endif

#endif /* E108_TEST_TASK_H */
