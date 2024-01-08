/*
 * but.c
 *
 *  Author: Demetriusz
 */
#include "led.h"

#include "config.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "menu_param.h"
#include "soc/mcpwm_periph.h"
#include "stdint.h"

#define MODULE_NAME "[LED] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_BUTTON
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define PWM_UNIT MCPWM_UNIT_0
#define PWM_DUTY _get_brighness()

float _get_brighness( void )
{
  uint32_t value = menuGetValue( MENU_BRIGHTNESS );
  if ( value < 1 || value >= 10 )
  {
    return 100.0;
  }
  else
  {
    return (float) value * 10.0;
  }
}

void set_motor_green_led( bool on_off )
{
  if ( on_off )
  {
    mcpwm_set_duty( MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_A, PWM_DUTY );
    mcpwm_set_duty_type( MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, MCPWM_DUTY_MODE_0 );
  }
  else
  {
    mcpwm_set_signal_low( MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A );
  }
}

void set_servo_green_led( bool on_off )
{
  if ( on_off )
  {
    mcpwm_set_duty( MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_B, PWM_DUTY );
    mcpwm_set_duty_type( MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_GEN_B, MCPWM_DUTY_MODE_0 );
  }
  else
  {
    mcpwm_set_signal_low( MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_B );
  }
}

void set_motor_red_led( bool on_off )
{
  if ( on_off )
  {
    mcpwm_set_duty( MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, PWM_DUTY );
    mcpwm_set_duty_type( MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, MCPWM_DUTY_MODE_0 );
  }
  else
  {
    mcpwm_set_signal_low( MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A );
  }
}

void set_servo_red_led( bool on_off )
{
  if ( on_off )
  {
    mcpwm_set_duty( MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B, PWM_DUTY );
    mcpwm_set_duty_type( MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B, MCPWM_DUTY_MODE_0 );
  }
  else
  {
    mcpwm_set_signal_low( MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B );
  }
}

void init_leds( void )
{
  mcpwm_gpio_init( MCPWM_UNIT_0, MCPWM0A, MOTOR_LED_RED );
  mcpwm_gpio_init( MCPWM_UNIT_0, MCPWM0B, SERVO_VIBRO_LED_RED );
  mcpwm_gpio_init( MCPWM_UNIT_0, MCPWM1A, MOTOR_LED_GREEN );
  mcpwm_gpio_init( MCPWM_UNIT_0, MCPWM1B, SERVO_VIBRO_LED_GREEN );
  mcpwm_config_t pwm_config = { 0 };

  pwm_config.frequency = 50;    // frequency = 1000Hz
  pwm_config.cmpr_a = 0;    // duty cycle of PWMxA = 60.0%
  pwm_config.cmpr_b = 0;    // duty cycle of PWMxb = 50.0%
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

  mcpwm_init( MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config );    // Configure PWM0A & PWM0B with above settings
  mcpwm_init( MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config );    // Configure PWM0A & PWM0B with above settings

  mcpwm_set_signal_low( MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A );
  mcpwm_set_signal_low( MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A );
  mcpwm_set_signal_low( MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B );
  mcpwm_set_signal_low( MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_B );
}
