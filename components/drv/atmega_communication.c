#include "atmega_communication.h"
#include "menu_param.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"

#include "freertos/timers.h"

#include "motor.h"
#include "servo.h"
#include "vibro.h"
#include "server_controller.h"


#define I2C_AT_ADDR 0x10
#ifndef PCF8574_I2C_PORT
#define PCF8574_I2C_PORT		I2C_NUM_0
#endif
#define ACK_CHECK_EN                        0x1              /*!< I2C master will check ack from slave*/
#define LAST_NACK_VAL                       0x2              /*!< I2C last_nack value */

#define at_send_data(data, len) i2c_send_data((uint8_t *)data, len)

static uint32_t byte_received;
static uint8_t cmd, data_len;
static uint16_t data_write[AT_W_LAST_POSITION];
static uint16_t data_read[AT_R_LAST_POSITION];
//static uint16_t data_cmd[AT_C_LAST_POSITION];

static TimerHandle_t xTimers;

static uint8_t i2c_send_data(uint8_t * data, uint8_t len) {
	int ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	ret = i2c_master_start(cmd);
	if (ret != ESP_OK)
	{
		debug_msg("i2c_master_start %d\n\r", ret)
	}
	ret = i2c_master_write_byte(cmd, I2C_AT_ADDR | I2C_MASTER_WRITE, ACK_CHECK_EN);
	if (ret != ESP_OK)
	{
		debug_msg("i2c_master_write_byte %d\n\r", ret)
	}
	ret = i2c_master_write(cmd, data, len, ACK_CHECK_EN);
	if (ret != ESP_OK)
	{
		debug_msg("i2c_master_write %d\n\r", ret)
	}
	ret = i2c_master_stop(cmd);
	if (ret != ESP_OK)
	{
		debug_msg("i2c_master_stop %d\n\r", ret)
	}
	ret = i2c_master_cmd_begin(PCF8574_I2C_PORT, cmd, 1000 / portTICK_RATE_MS);
	if (ret != ESP_OK)
	{
		debug_msg("i2c_master_cmd_begin %d\n\r", ret)
	}
	i2c_cmd_link_delete(cmd);
	return ret;
}

static uint8_t i2c_read_data(uint8_t * data, uint8_t len) {
	int ret = 0;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, I2C_AT_ADDR | I2C_MASTER_READ, ACK_CHECK_EN);
	i2c_master_read(cmd, (uint8_t*)&data, 1, LAST_NACK_VAL);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(PCF8574_I2C_PORT, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

static void clear_msg(void) {
	byte_received = 0;
	cmd = 0;
	data_len = 0;
	xTimerStop(xTimers, 0);
}

static void vTimerCallback( TimerHandle_t xTimer )
{
	(void)xTimer;
	clear_msg();
	//debug_msg("vTimerCallback\n\r");
}

uint8_t send_buff[256];

void at_write_data(void) {
	uint8_t *pnt = (uint8_t *)data_write;

	send_buff[0] = START_FRAME_WRITE;
	send_buff[1] = sizeof(data_write);

	for (uint16_t i = 0; i < sizeof(data_write); i++) {
		send_buff[2 + i] = pnt[i];
	}
	at_send_data(send_buff, sizeof(data_write) + 2);
}

uint16_t atmega_get_data(atmega_data_read_t data_type) {
	if (data_type < AT_R_LAST_POSITION) {
		return data_read[data_type];
	}
	return 0;
}

static void atmega_set_read_data(void) {
	
}

void at_read_byte(uint8_t byte) {
	if (byte_received == 0) {
		if (byte == START_FRAME_WRITE || byte == START_FRAME_READ || byte == START_FRAME_CMD) {
			cmd = byte;
			byte_received++;
			xTimerStart( xTimers, 0 );
			return;
		}
		else {
			/* Debug logs */
			debug_data(&byte, 1);
		}
	}

	switch(cmd) {
		case START_FRAME_WRITE:
			if (byte_received == 1) {
				data_len = byte;
				byte_received++;
				if (data_len != sizeof(data_read)) {
					clear_msg();
					debug_msg("FRAME BAD LEN\n\r");
				}
				xTimerStart( xTimers, 0 );
			}
			else if (byte_received - 2 < data_len) {
				uint8_t *pnt = (uint8_t *)data_read;
				pnt[byte_received - 2] = byte;
				byte_received++;
				if (byte_received - 2 == data_len) {
					/* Verify data */
					clear_msg();
				}
				xTimerStart( xTimers, 0 );
			}
			else {
				clear_msg();
				//debug_msg("ATMEGA RECEIVE UNKNOW ERROR\n\r");
			}
			break;

		case START_FRAME_READ:
			/* SEND BUFF data_write */
			at_write_data();
			clear_msg();
			break;

		case START_FRAME_CMD:
			/* Nothing for host */
			clear_msg();
			break;

		default:
			clear_msg();
	}
}

static void read_uart_data(void * arg)
{
	char input;
	while (1)
	{
		if (uart_read_bytes(UART_NUM_0, (uint8_t *)&input, 1, 100) > 0) {
			at_read_byte((uint8_t) input);
		}
	}
}

static void atm_com(void * arg) {
	uint16_t motor_pmw, servo_pwm;
	servo_init(0);
	while(1) {
		// vTaskDelay(150 / portTICK_RATE_MS);
		// taskENTER_CRITICAL();
		// data_write[AT_W_MOTOR_VALUE] = srvrControllGetMotorPwm();
		// #if CONFIG_DEVICE_SIEWNIK
		// data_write[AT_W_SERVO_VALUE] = srvrControllGetServoPwm();
		// data_write[AT_W_SERVO_IS_ON] = srvrControllGetServoStatus();
		// #endif

		// #if CONFIG_DEVICE_SOLARKA
		// vibro_config(menuGetValue(MENU_VIBRO_PERIOD) * 1000, menuGetValue(MENU_VIBRO_WORKING_TIME) * 1000);
		// if (menuGetValue(MENU_SERVO_IS_ON)) {
		// 	vibro_start();
		// }
		// else {
		// 	vibro_stop();
		// }
		// data_write[AT_W_SERVO_IS_ON] = (uint16_t)vibro_is_on();
		// #endif
		
		// data_write[AT_W_MOTOR_IS_ON] = srvrControllGetMotorStatus();
		// data_write[AT_W_SYSTEM_ON] = !(srvrControllGetEmergencyDisable());

		// at_write_data();
		// taskEXIT_CRITICAL();
		// debug_msg("mot: %d %d, servo: %d %d system: %d\n\r", data_write[AT_W_MOTOR_VALUE], data_write[AT_W_MOTOR_IS_ON], data_write[AT_W_SERVO_IS_ON], data_write[AT_W_SERVO_VALUE], data_write[AT_W_SYSTEM_ON]);

		vTaskDelay(1000 / portTICK_RATE_MS);
		char * data_i2c_write = "help";
		char data_read[8];
		int ret = i2c_send_data((uint8_t *)data_i2c_write, 4);
		debug_msg("i2c_send_data value = %d\n\r", ret);
		ret = i2c_read_data((uint8_t *)data_read, 4);
		ESP_LOGI("COM", "i2c_read_data value = %d, read %d\n\r", ret, data_read[0]);

	}
}

void at_communication_init(void) {
	xTaskCreate(atm_com, "atm_com", 1024, NULL, 10, NULL);
	xTaskCreate(read_uart_data, "read_uart_data", 1024, NULL, 10, NULL);
	xTimers = xTimerCreate("at_com_tim", 100 / portTICK_RATE_MS, pdFALSE, ( void * ) 0, vTimerCallback);
}