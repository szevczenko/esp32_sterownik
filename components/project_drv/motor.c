#include "motor.h"

#include <stdio.h>

#include "app_config.h"

#include "parameters.h"

#undef printf
#define printf( ... )

#define LED_MOTOR_OFF
#define LED_MOTOR_ON
#define CMD_MOTOR_OFF
#define CMD_MOTOR_ON
#define CMD_MOTOTR_SET_PWM( pwm )

/*
 * init a motor
 */
void motor_init( mDriver* motorD )
{
  printf( "dcmotor init\n" );
  motorD->state = MOTOR_OFF;
  LED_MOTOR_OFF;
  CMD_MOTOR_OFF;
}

void motor_deinit( mDriver* motorD )
{
  //printf("dcmotor deinit\n");
  motorD->state = MOTOR_NO_INIT;
  CMD_MOTOR_OFF;
  LED_MOTOR_OFF;
}

/*
 * stop the motor
 */
int motor_stop( mDriver* motorD )
{
  //set orc
  if ( dcmotor_is_on( motorD ) )
  {
    printf( "dcmotor stop\n" );
    CMD_MOTOR_OFF;
    LED_MOTOR_OFF;
    motorD->last_state = motorD->state;
    motorD->state = MOTOR_OFF;
    return 1;
  }
  else
  {
    //printf("dcmotor cannot stop\n");
  }

  return 0;
}

int dcmotor_is_on( mDriver* motorD )
{
  if ( ( motorD->state == MOTOR_ON ) || ( motorD->state == MOTOR_AXELERATE ) || ( motorD->state == MOTOR_TRY ) )
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int motor_start( mDriver* motorD )
{
  if ( motorD->state == MOTOR_OFF )
  {
    //printf("Motor Start\n");
    LED_MOTOR_ON;
    CMD_MOTOR_ON;
    motorD->last_state = motorD->state;
    if ( motorD->pwm >= 40 )
    {
      motorD->state = MOTOR_AXELERATE;
      motorD->timeout = xTaskGetTickCount() + MS2ST( 1000 );
    }
    else
    {
      motorD->state = MOTOR_ON;
    }

    return 1;
  }
  else
  {
    //printf("dcmotor canot start\n");
    return 0;
  }
}

int dcmotor_set_pwm( mDriver* motorD, float pwm )
{
  float pwm_set = 0;

  if ( pwm > 100 )
  {
    printf( "dcmotor_set_pwm > 100 %f\n\r", pwm );
    pwm = 100;
  }

  if ( pwm < 0 )
  {
    printf( "dcmotor_set_pwm < 0 %f\n\r", pwm );
    pwm = 0;
  }

  if ( pwm == 0 )
  {
    motorD->pwm_value = 0;
    return 1;
  }

  printf( "dcmotor_set_pwm %f\n", pwm );
  float min_value = (float) parameters_getValue( PARAM_MOTOR_MIN_CALIBRATION );
  float max_value = (float) parameters_getValue( PARAM_MOTOR_MAX_CALIBRATION );
  float range = 100.0;

  if ( min_value > max_value )
  {
    printf( "dcmotor_set_pwm min_value > max_value\n\r" );
    min_value = parameters_getValue( PARAM_MOTOR_MIN_CALIBRATION );
    max_value = parameters_getValue( PARAM_MOTOR_MAX_CALIBRATION );
  }

  pwm_set = ( max_value - min_value ) * pwm / range + min_value;
  motorD->pwm_value = pwm_set;
  CMD_MOTOTR_SET_PWM( count_pwm( motorD->pwm_value ) );
  return 1;
}

int dcmotor_get_pwm( mDriver* motorD )
{
  return motorD->pwm_value;
}

void dcmotor_set_error( mDriver* motorD )
{
  printf( "dcmotor error\n" );
  motor_stop( motorD );
  motorD->state = MOTOR_ERROR;
}

int dcmotor_set_try( mDriver* motorD )
{
  if ( dcmotor_is_on( motorD ) )
  {
    motorD->state = MOTOR_TRY;
    return 1;
  }

  return 0;
}

int dcmotor_set_normal_state( mDriver* motorD )
{
  if ( dcmotor_is_on( motorD ) )
  {
    motorD->state = MOTOR_ON;
    return 1;
  }

  return 0;
}

void motor_regulation( mDriver* motorD, float pwm )
{
  motorD->state = MOTOR_REGULATION;
  motorD->pwm_value = pwm;
}

float dcmotor_process( mDriver* motorD, uint8_t value )
{
  motorD->pwm = value;
  switch ( motorD->state )
  {
    case MOTOR_ON:
      printf( "MOTOR_ON %d\n\r", value );
      dcmotor_set_pwm( motorD, (float) value );
      break;

    case MOTOR_OFF:
      printf( "MOTOR_OFF %d\n\r", value );
      motorD->pwm_value = 0;
      break;

    case MOTOR_TRY:
      printf( "MOTOR_TRY %d\n\r", value );
      if ( value <= 50 )
      {
        dcmotor_set_pwm( motorD, value + 20 );
      }
      else if ( ( value > 50 ) && ( value <= 70 ) )
      {
        dcmotor_set_pwm( motorD, value + 15 );
      }
      else
      {
        dcmotor_set_pwm( motorD, value );
      }

      break;

    case MOTOR_ERROR:
      printf( "MOTOR_ERROR %d\n\r", value );
      CMD_MOTOR_OFF;
      break;

    case MOTOR_AXELERATE:
      printf( "MOTOR_AXELERATE %d\n\r", value );
      // motorD->state = MOTOR_ON;    //!!
      // break;                       //!
      dcmotor_set_pwm( motorD, 40 );

      //printf("MOTOR_AXELERATE %d\n", motorD->pwm_value);
      if ( motorD->timeout < xTaskGetTickCount() )
      {
        motorD->state = MOTOR_ON;
      }

      break;

    case MOTOR_REGULATION:
      printf( "MOTOR_REGULATION %d\n\r", value );
      dcmotor_set_pwm( motorD, value );
      break;

    default:
      printf( "MOTOR_ERROR_STATE %d\n\r", motorD->state );
      break;
  }

  return motorD->pwm_value;
}
