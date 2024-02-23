#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "led.h"
#include "lwip/arch.h"
#include "dev_config.h"

#ifndef SSD1306_I2C_PORT
#define SSD1306_I2C_PORT I2C_NUM_0
#endif

#ifndef SSD1306_I2C_ADDR
#define SSD1306_I2C_ADDR 0x3C
#endif

#define I2C_MASTER_SCL_IO 22 /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 21 /*!< gpio number for I2C master data  */

// SSD1306 OLED height in pixels
#ifndef SSD1306_HEIGHT
#define SSD1306_HEIGHT 64
#endif

// SSD1306 width in pixels
#ifndef SSD1306_WIDTH
#define SSD1306_WIDTH 128
#endif

#define CFG_VALVE_CNT                    7
#define CFG_VALVE_CURRENT_REGULATION_PIN 27

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef ERROR
#define ERROR -1
#endif

#ifndef TIMEOUT
#define TIMEOUT -2
#endif

#define CONFIG_DEVICE_SIEWNIK FALSE
#define CONFIG_DEVICE_SOLARKA TRUE

#define T_DEV_TYPE_SIEWNIK 1
#define T_DEV_TYPE_SOLARKA 2
#define T_DEV_TYPE_VALVE   3

///////////////////// LOGS //////////////////////

#define CONFIG_DEBUG_CMD_CLIENT        TRUE
#define CONFIG_DEBUG_CMD_SERVER        TRUE
#define CONFIG_DEBUG_PARSE_CMD         TRUE
#define CONFIG_DEBUG_WIFI              TRUE
#define CONFIG_DEBUG_BATTERY           TRUE
#define CONFIG_DEBUG_BUTTON            TRUE
#define CONFIG_DEBUG_ERROR_SIEWNIK     TRUE
#define CONFIG_DEBUG_KEEP_ALIVE        TRUE
#define CONFIG_DEBUG_MEASURE           TRUE
#define CONFIG_DEBUG_SERVER_CONTROLLER TRUE
#define CONFIG_DEBUG_MENU_BACKEND      TRUE
#define CONFIG_DEBUG_SLEEP             TRUE

/////////////////////  CONFIG PERIPHERALS  ////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
// CONSOLE
#define CONFIG_CONSOLE_VSNPRINTF_BUFF_SIZE 64
#define CONFIG_CONSOLE_SERIAL_SPEED        115200

///////////////////////////////////////////////////////////////////////////////////////////
//// LED
#define MOTOR_LED_RED         GPIO_NUM_15
#define SERVO_VIBRO_LED_RED   GPIO_NUM_12
#define MOTOR_LED_GREEN       GPIO_NUM_25
#define SERVO_VIBRO_LED_GREEN GPIO_NUM_26

#define MOTOR_LED_SET_RED( x )         set_motor_red_led( x );
#define SERVO_VIBRO_LED_SET_RED( x )   set_servo_red_led( x );
#define MOTOR_LED_SET_GREEN( x )       set_motor_green_led( x );
#define SERVO_VIBRO_LED_SET_GREEN( x ) set_servo_green_led( x );

//////////////////////////////////////  END  //////////////////////////////////////////////

#define NORMALPRIO       5

#define MS2ST( ms )   pdMS_TO_TICKS( ms )
#define ST2MS( tick ) ( ( tick ) * portTICK_PERIOD_MS )

#define osDelay( ms )       vTaskDelay( MS2ST( ms ) )
#define debug_printf( ... ) DevConfig_Printf( __VA_ARGS__ )

#endif /* APP_CONFIG_H_ */
