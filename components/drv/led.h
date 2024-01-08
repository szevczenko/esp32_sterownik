#ifndef _LED_H_
#define _LED_H_

#include <stdbool.h>

void set_motor_green_led( bool on_off );
void set_servo_green_led( bool on_off );
void set_motor_red_led( bool on_off );
void set_servo_red_led( bool on_off );

void init_leds( void );

#endif