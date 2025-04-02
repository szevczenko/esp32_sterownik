#ifndef _TANK_SENSOR_H_
#define _TANK_SENSOR_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Check if tank sensor is connected.
 * 
 * @return true if connected, false otherwise
 */
bool tank_sensor_is_connected( void );

/**
 * @brief Get distance from tank sensor.
 * 
 * @return distance in mm
 */
uint32_t tank_sensor_get_distance( void );

/**
 * @brief Get percentage of tank level.
 * 
 * @return percentage of tank level
 */
int tank_sensor_get_percent( void );

/**
 * @brief Get volume from tank sensor.
 * 
 * @return volume in liters
 */
float tank_sensor_get_volume(void);

/**
 * @brief Get flow rate from tank sensor.
 * 
 * @return flow rate in liters per minute
 */
float tank_sensor_get_flow_rate(void);

/**
 * @brief Initialize tank sensor.
 */
void tank_sensor_init( void );

#endif