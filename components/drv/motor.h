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
	motorState state;
	motorState last_state;
	uint8_t error_code;
	float pwm_value;
	TickType_t timeout;
	uint8_t try_cnt;
	
} mDriver;

//functions
extern void motor_init(mDriver *motorD);
void motor_deinit(mDriver *motorD);
extern int motor_stop(mDriver *motorD);
extern int motor_start(mDriver *motorD);
int motor_start(mDriver *motorD);
extern int dcmotor_is_on(mDriver *motorD);
float dcmotor_process(mDriver *motorD, uint8_t value);
void dcmotor_set_error(mDriver *motorD);
int dcmotor_set_try(mDriver *motorD);
int dcmotor_set_normal_state(mDriver *motorD);
int dcmotor_get_pwm(mDriver *motorD);
void motor_regulation(mDriver *motorD, float pwm);

#endif