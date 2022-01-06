#ifndef ATMEGA_COMMUNICATION_H
#define ATMEGA_COMMUNICATION_H
#include "config.h"


#define START_FRAME_WRITE 0xDE
#define START_FRAME_READ 0xFE
#define START_FRAME_CMD 0xEE

#define END_FRAME_WRITE 0xBE
#define END_FRAME_READ 0xFA
#define END_FRAME_CMD 0xEA

typedef enum
{
	AT_R_MEAS_ACCUM,
	AT_R_MEAS_MOTOR,
	AT_R_MEAS_SERVO,
	AT_R_LAST_POSITION,
}atmega_data_read_t;

typedef enum
{
	AT_W_MOTOR_VALUE,
	AT_W_SERVO_VALUE,
	AT_W_MOTOR_IS_ON,
	AT_W_SERVO_IS_ON,
	AT_W_SYSTEM_ON,
	AT_W_LAST_POSITION,
}atmega_data_write_t;

typedef enum
{
	AT_C_MOTOR_STATUS,
	AT_C_SERVO_STATUS,
	AT_C_LAST_POSITION,
}atmega_data_cmd_t;

void at_communication_init(void);
void at_read_byte(uint8_t byte);
uint16_t atmega_get_data(atmega_data_read_t data_type);

#endif