#ifndef motor_H
#define motor_H
#include "stdint.h"
#include "config.h"
#include "freertos/timers.h"

//set minimum velocity
#if CONFIG_DEVICE_SOLARKA
#define motor_MINVEL 200
#endif

#if CONFIG_DEVICE_SIEWNIK
#define motor_MINVEL 125
#endif


typedef enum
{
	MOTOR_NO_INIT = 0,
	MOTOR_OFF,
	MOTOR_ON,
	MOTOR_TRY,
	MOTOR_AXELERATE,
	MOTOR_ERROR ,
	MOTOR_REGULATION,
}motorState;

typedef struct  
{
	uint8_t state;
	uint8_t last_state;
	uint8_t error_code;
	uint8_t pwm_value;
	TickType_t timeout;
	uint8_t try_cnt;
	
}mDriver;

//functions
extern void motor_init(void);
void motor_deinit(void);
extern int motor_stop(void);
extern int motor_start(void);
int motor_start(void);
extern int dcmotor_is_on(void);
uint8_t dcmotor_process(uint8_t value);
void dcmotor_set_error(void);
int dcmotor_set_try(void);
int dcmotor_set_normal_state(void);
int dcmotor_get_pwm(void);
void motor_goforward(uint8_t vel);
void motor_gobackward(uint8_t vel);
void motor_regulation(uint8_t pwm);

#endif