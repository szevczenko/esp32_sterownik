/*
 * pwm.h
 *
 * Created: 07.02.2019 13:19:21
 *  Author: Demetriusz
 */

#ifndef PWM_H_
#define PWM_H_
#include "config.h"

//#define SERVO_PORT DDRD
//#define SERVO_PIN
#define PWM_CLOSED                 0
#define PWM_OPEN                   100
#define TRY_OPEN_VAL               10
#define TIME_AFTER_RESET_SERVO_TRY 5000

#if CONFIG_DEVICE_SIEWNIK

#define CLOSE_SERVO     servo_set_pwm_val( 0 );
#define TickType_tSERVO set_pwm( 19999 );

typedef enum
{
  SERVO_NO_INIT = 0,
  SERVO_CLOSE,
  SERVO_OPEN,
  SERVO_REGULATION,
  SERVO_TRY,
  SERVO_ERROR_PROCESS,
  SERVO_ERROR
} SERVOState;

typedef struct
{
  uint8_t state;
  uint8_t last_state;
  uint8_t error_code;
  uint16_t pwm_value;    // PWM 16bit timer
  uint8_t value;    // Open procent timer
  TickType_t timeout;
  uint8_t try_cnt;
} sDriver;

void servo_init( uint8_t prescaler );
void servo_error( uint8_t close );
uint16_t servo_process( uint8_t value );
int servo_close( void );
void servo_enable_try( void );
int servo_open( uint8_t value );
int servo_is_open( void );
void servo_try_reset_timeout( uint32_t time_ms );
int servo_get_try_cnt( void );
void servo_regulation( uint8_t value );

#endif

#endif /* PWM_H_ */