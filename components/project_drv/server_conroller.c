#include <stdbool.h>

#include "cmd_server.h"
#include "error_siewnik.h"
#include "error_solarka.h"
#include "http_server.h"
#include "measure.h"
#include "motor.h"
#include "parameters.h"
#include "parse_cmd.h"
#include "pwm_drv.h"
#include "server_controller.h"
#include "servo.h"
#include "vibro.h"

#define MODULE_NAME "[Srvr Ctrl] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_SERVER_CONTROLLER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define SYSTEM_ON_PIN 15

#define MOTOR_PWM_PIN  27
#define VIBRO_PWM_PIN  25
#define SERVO_PWM_PIN  26
#define MOTOR_PWM_PIN2 25

typedef enum
{
  STATE_INIT,
  STATE_IDLE,
  STATE_LOW_VOLTAGE,
  STATE_WORKING,
  STATE_SERVO_OPEN_REGULATION,
  STATE_SERVO_CLOSE_REGULATION,
  STATE_MOTOR_REGULATION,
  STATE_EMERGENCY_DISABLE,
  STATE_ERROR,
  STATE_LAST,
} state_t;

typedef struct
{
  state_t state;
  mDriver motorD1;
  mDriver motorD2;
  uint8_t servo_value;
  uint8_t servo_new_value;
  uint8_t servo_set_value;
  uint32_t servo_set_timer;
  uint8_t motor_value;

  uint8_t motor_on;
  uint8_t servo_on;
  uint16_t servo_pwm;
  float motor_pwm;
  float motor_pwm2;
  bool system_on;
  bool emergency_disable;
  bool errors;

  bool working_state_req;
  bool motor_calibration_req;
  bool servo_open_calibration_req;
  bool servo_close_calibration_req;
  pwm_drv_t motor1_pwm;
  pwm_drv_t motor2_pwm;
  pwm_drv_t servo_pwm_drv;
} server_conroller_ctx;

static server_conroller_ctx ctx;

static bool test_last_motor_state;

static char* state_name[] =
  {
    [STATE_INIT] = "STATE_INIT",
    [STATE_IDLE] = "STATE_IDLE",
    [STATE_LOW_VOLTAGE] = "STATE_LOW_VOLTAGE",
    [STATE_WORKING] = "STATE_WORKING",
    [STATE_SERVO_OPEN_REGULATION] = "STATE_SERVO_OPEN_REGULATION",
    [STATE_SERVO_CLOSE_REGULATION] = "STATE_SERVO_CLOSE_REGULATION",
    [STATE_MOTOR_REGULATION] = "STATE_MOTOR_REGULATION",
    [STATE_EMERGENCY_DISABLE] = "STATE_EMERGENCY_DISABLE",
    [STATE_ERROR] = "STATE_ERROR" };

static void change_state( state_t state )
{
  if ( state >= STATE_LAST )
  {
    return;
  }

  if ( state != ctx.state )
  {
    LOG( PRINT_INFO, "Change state -> %s", state_name[state] );
    ctx.state = state;
  }
}

static void count_working_data( void )
{
  ctx.motor_pwm = dcmotor_process( &ctx.motorD1, ctx.motor_value );
  ctx.motor_pwm2 = dcmotor_process( &ctx.motorD2, ctx.motor_value );
#if CONFIG_DEVICE_SIEWNIK
  if ( ctx.servo_new_value != ctx.servo_value )
  {
    ctx.servo_new_value = ctx.servo_value;
    ctx.servo_set_timer = xTaskGetTickCount() + MS2ST( 750 );
    errorSiewnikServoChangeState();
  }
#endif

  if ( ctx.motor_on )
  {
    motor_start( &ctx.motorD1 );
    motor_start( &ctx.motorD2 );
  }
  else
  {
    motor_stop( &ctx.motorD1 );
    motor_stop( &ctx.motorD2 );
  }

#if CONFIG_DEVICE_SIEWNIK
  if ( ctx.servo_set_timer < xTaskGetTickCount() )
  {
    ctx.servo_set_value = ctx.servo_new_value;
  }
  ctx.servo_pwm = servo_process( ctx.servo_on ? ctx.servo_set_value : 0 );
#endif
}

static void set_working_data( void )
{
  // #if CONFIG_DEVICE_SIEWNIK

  if ( ctx.system_on )
  {
    gpio_set_level( SYSTEM_ON_PIN, 1 );
  }
  else
  {
    gpio_set_level( SYSTEM_ON_PIN, 0 );
  }

  LOG( PRINT_DEBUG, "motor %d %f %d", ctx.motor_on, ctx.motor_pwm, ctx.motor_value );
  if ( ctx.motor_on )
  {
    float duty = (float) ctx.motor_pwm;
    LOG( PRINT_DEBUG, "duty motor %f", duty );
    if ( duty >= 99.99 )
    {
      duty = 99.99;
    }
    PWMDrv_SetDuty( &ctx.motor1_pwm, duty );
  }
  else
  {
#if CONFIG_DEVICE_SIEWNIK
    PWMDrv_Stop( &ctx.motor1_pwm, true );
    PWMDrv_Stop( &ctx.motor2_pwm, true );
#endif

#if CONFIG_DEVICE_SOLARKA
    PWMDrv_Stop( &ctx.motor1_pwm, false );
#endif
  }

#if CONFIG_DEVICE_SOLARKA
  if ( vibro_is_on() && ctx.servo_on )
  {
    // ToDo napiecie 2 progi
    PWMDrv_SetDuty( &ctx.servo_pwm_drv, parameters_getValue( PARAM_VIBRO_DUTY_PWM ) );
  }
  else
  {
    PWMDrv_Stop( &ctx.servo_pwm_drv, true );
  }
#endif

#if CONFIG_DEVICE_SIEWNIK
  float duty = (float) ctx.servo_pwm * 100 / 19999.0;
  if ( ( parameters_getValue( PARAM_MACHINE_ERRORS ) & ( 1 << ERROR_SERVO_OVER_CURRENT ) ) || ( ctx.state == STATE_IDLE ) )
  {
    duty = 99.99;
  }
  LOG( PRINT_DEBUG, "duty servo %f %d %d", duty, ctx.servo_value, ctx.servo_pwm );
  PWMDrv_SetDuty( &ctx.servo_pwm_drv, duty );
#endif
}

static void state_init( void )
{
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = ( 1 << SYSTEM_ON_PIN ) | ( 1 << VIBRO_PWM_PIN );
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config( &io_conf );

#if CONFIG_DEVICE_SOLARKA
  PWMDrv_Init( &ctx.motor1_pwm, "motor1_pwm", PWM_DRV_DUTY_MODE_HIGH, 16000, 0, MOTOR_PWM_PIN );
  PWMDrv_Init( &ctx.servo_pwm_drv, "servo_pwm", PWM_DRV_DUTY_MODE_LOW, 16000, 1, VIBRO_PWM_PIN );
  PWMDrv_Stop( &ctx.servo_pwm_drv, true );
  PWMDrv_Stop( &ctx.motor1_pwm, false );
#endif

#if CONFIG_DEVICE_SIEWNIK
  PWMDrv_Init( &ctx.motor1_pwm, "motor1_pwm", PWM_DRV_DUTY_MODE_LOW, 16000, 0, MOTOR_PWM_PIN );
  PWMDrv_Init( &ctx.motor2_pwm, "motor2_pwm", PWM_DRV_DUTY_MODE_LOW, 16000, 0, MOTOR_PWM_PIN2 );
  PWMDrv_Init( &ctx.servo_pwm_drv, "servo_pwm", PWM_DRV_DUTY_MODE_HIGH, 50, 1, SERVO_PWM_PIN );
#endif

  change_state( STATE_IDLE );
}

static void state_idle( void )
{
  ctx.servo_value = 0;
  ctx.motor_value = 0;
  ctx.motor_on = false;
  ctx.servo_on = false;

  ctx.working_state_req = (bool) parameters_getValue( PARAM_START_SYSTEM );
  ctx.emergency_disable = (bool) parameters_getValue( PARAM_EMERGENCY_DISABLE );
  vibro_stop();

  if ( ctx.emergency_disable )
  {
    change_state( STATE_EMERGENCY_DISABLE );
    return;
  }

  if ( ctx.working_state_req && HTTPServer_IsClientConnected() )
  {
    measure_meas_calibration_value();
    count_working_data();
    ctx.system_on = (bool) parameters_getValue( PARAM_START_SYSTEM );
    set_working_data();
    osDelay( 1000 );
    change_state( STATE_WORKING );
    return;
  }

  osDelay( 100 );
}

static void state_working( void )
{
  ctx.system_on = (bool) parameters_getValue( PARAM_START_SYSTEM );
  ctx.servo_value = (uint8_t) parameters_getValue( PARAM_SERVO );
  ctx.motor_value = (uint8_t) parameters_getValue( PARAM_MOTOR );
  ctx.motor_on = (uint16_t) parameters_getValue( PARAM_MOTOR_IS_ON );
  ctx.servo_on = parameters_getValue( PARAM_SERVO_IS_ON ) > 0;

  ctx.working_state_req = (bool) parameters_getValue( PARAM_START_SYSTEM );
  ctx.emergency_disable = (bool) parameters_getValue( PARAM_EMERGENCY_DISABLE );
  ctx.servo_open_calibration_req = (bool) parameters_getValue( PARAM_OPEN_SERVO_REGULATION_FLAG );
  ctx.servo_close_calibration_req = (bool) parameters_getValue( PARAM_CLOSE_SERVO_REGULATION_FLAG );

#if CONFIG_DEVICE_SOLARKA
#if MENU_VIRO_ON_OFF_VERSION
  vibro_config( parameters_getValue( PARAM_VIBRO_ON_S ) * 1000, parameters_getValue( PARAM_VIBRO_OFF_S ) * 1000 );
#else
  vibro_config( parameters_getValue( PARAM_PERIOD ) * 1000, parameters_getValue( PARAM_SERVO ) );
#endif
  if ( parameters_getValue( PARAM_SERVO_IS_ON ) )
  {
    vibro_start();
  }
  else
  {
    vibro_stop();
  }
#endif

  if ( ctx.emergency_disable )
  {
    vibro_stop();
    change_state( STATE_EMERGENCY_DISABLE );
    return;
  }

  if ( !ctx.working_state_req || !HTTPServer_IsClientConnected() )
  {
    vibro_stop();
    change_state( STATE_IDLE );
    return;
  }

  if ( ctx.servo_open_calibration_req )
  {
    vibro_stop();
    change_state( STATE_SERVO_OPEN_REGULATION );
    return;
  }

  if ( ctx.servo_close_calibration_req )
  {
    vibro_stop();
    change_state( STATE_SERVO_CLOSE_REGULATION );
    return;
  }

  osDelay( 50 );
}

static void state_servo_open_regulation( void )
{
  ctx.system_on = 1;
  ctx.servo_value = 100;
  ctx.motor_value = 0;
  ctx.motor_on = 0;
  ctx.servo_on = 1;

  ctx.working_state_req = (bool) parameters_getValue( PARAM_START_SYSTEM );
  ctx.emergency_disable = (bool) parameters_getValue( PARAM_EMERGENCY_DISABLE );
  ctx.servo_open_calibration_req = (bool) parameters_getValue( PARAM_OPEN_SERVO_REGULATION_FLAG );

  if ( ctx.emergency_disable )
  {
    parameters_save();
    change_state( STATE_EMERGENCY_DISABLE );
    return;
  }

  if ( !ctx.servo_open_calibration_req )
  {
    parameters_save();
    change_state( STATE_IDLE );
    return;
  }

  if ( !ctx.working_state_req || !HTTPServer_IsClientConnected() )
  {
    parameters_save();
    parameters_setValue( PARAM_OPEN_SERVO_REGULATION_FLAG, 0 );
    change_state( STATE_IDLE );
    return;
  }

  osDelay( 100 );
}

static void state_servo_close_regulation( void )
{
  ctx.system_on = 1;
  ctx.servo_value = 0;
  ctx.motor_value = 0;
  ctx.motor_on = 0;
  ctx.servo_on = 1;

  ctx.working_state_req = (bool) parameters_getValue( PARAM_START_SYSTEM );
  ctx.emergency_disable = (bool) parameters_getValue( PARAM_EMERGENCY_DISABLE );
  ctx.servo_close_calibration_req = (bool) parameters_getValue( PARAM_CLOSE_SERVO_REGULATION_FLAG );

  if ( ctx.emergency_disable )
  {
    parameters_save();
    change_state( STATE_EMERGENCY_DISABLE );
    return;
  }

  if ( !ctx.servo_close_calibration_req )
  {
    parameters_save();
    change_state( STATE_IDLE );
    return;
  }

  if ( !ctx.working_state_req || !HTTPServer_IsClientConnected() )
  {
    parameters_save();
    parameters_setValue( PARAM_OPEN_SERVO_REGULATION_FLAG, 0 );
    change_state( STATE_IDLE );
    return;
  }

  osDelay( 100 );
}

static void state_motor_regulation( void )
{
  change_state( STATE_IDLE );
}

static void state_emergency_disable( void )
{
  // Tą linijke usunąć jeżeli niepotrzebne wyłączenie przekaźnika w trybie STOP
  ctx.system_on = 0;
  ctx.emergency_disable = (bool) parameters_getValue( PARAM_EMERGENCY_DISABLE );
  ctx.servo_value = 0;
  ctx.motor_value = 0;
  ctx.motor_on = false;
  ctx.servo_on = false;

  if ( !ctx.emergency_disable )
  {
    change_state( STATE_IDLE );
    return;
  }

  osDelay( 100 );
}

static void state_error( void )
{
  ctx.errors = (bool) parameters_getValue( PARAM_MACHINE_ERRORS );
  ctx.servo_value = 0;
  ctx.motor_value = 0;
  ctx.motor_on = false;
  ctx.servo_on = false;

  if ( !ctx.errors )
  {
#if CONFIG_DEVICE_SIEWNIK
    errorSiewnikErrorReset();
#endif

#if CONFIG_DEVICE_SOLARKA
    errorSolarkaErrorReset();
#endif
    change_state( STATE_IDLE );
    return;
  }

  osDelay( 100 );
}

static void _task( void* arg )
{
  while ( 1 )
  {
    switch ( ctx.state )
    {
      case STATE_INIT:
        state_init();
        break;

      case STATE_IDLE:
        state_idle();
        break;

      case STATE_WORKING:
        state_working();
        break;

      case STATE_SERVO_OPEN_REGULATION:
        state_servo_open_regulation();
        break;

      case STATE_SERVO_CLOSE_REGULATION:
        state_servo_close_regulation();
        break;

      case STATE_MOTOR_REGULATION:
        state_motor_regulation();
        break;

      case STATE_EMERGENCY_DISABLE:
        state_emergency_disable();
        break;

      case STATE_ERROR:
        state_error();
        break;

      case STATE_LOW_VOLTAGE:
        ctx.servo_value = 0;
        ctx.motor_value = 0;
        ctx.motor_on = false;
        ctx.servo_on = false;
        float voltage = accum_get_voltage();
        // printf("voltage: %f\n\r", voltage);
        if ( 5 < voltage )
        {
          change_state( STATE_IDLE );
        }
        break;

      default:
        change_state( STATE_IDLE );
        break;
    }
    float voltage = accum_get_voltage();
    // printf("voltage: %f\n\r", voltage);
    if ( 5 > voltage )
    {
      // change_state(STATE_LOW_VOLTAGE);
    }
    count_working_data();
    set_working_data();

    //TEST
    if ( ctx.motor_on != test_last_motor_state )
    {
      test_last_motor_state = ctx.motor_on;
      if ( ctx.motor_on )
      {
        LOG( PRINT_DEBUG, "----MOTOR ON" );
      }
      else
      {
        LOG( PRINT_DEBUG, "----MOTOR OFF" );
      }
    }
  }
}

bool srvrControllIsWorking( void )
{
  return ctx.state == STATE_WORKING;
}

bool srvrControllGetMotorStatus( void )
{
  return ctx.motor_on;
}

bool srvrControllGetServoStatus( void )
{
  return ctx.servo_on;
}

uint8_t srvrControllGetMotorPwm( void )
{
  return ctx.motor_pwm;
}

uint16_t srvrControllGetServoPwm( void )
{
  return ctx.servo_pwm;
}

bool srvrControllGetEmergencyDisable( void )
{
  return ctx.emergency_disable;
}

void srvrControllStart( void )
{
  motor_init( &ctx.motorD1 );
  motor_init( &ctx.motorD2 );
#if CONFIG_DEVICE_SIEWNIK
  servo_init( 0 );
#endif

#if CONFIG_DEVICE_SOLARKA
  vibro_init();
#endif

  xTaskCreate( _task, "srvrController", 4096, NULL, 10, NULL );
}

bool srvrConrollerSetError( uint16_t error_reason )
{
  if ( ctx.state == STATE_WORKING )
  {
    change_state( STATE_ERROR );
    uint16_t error = ( 1 << error_reason );
    parameters_setValue( PARAM_MACHINE_ERRORS, error );
    return true;
  }

  return false;
}

bool srvrControllerErrorReset( void )
{
  if ( ctx.state == STATE_ERROR )
  {
#if CONFIG_DEVICE_SIEWNIK
    errorSiewnikErrorReset();
#endif

#if CONFIG_DEVICE_SOLARKA
    errorSolarkaErrorReset();
#endif

    change_state( STATE_IDLE );
    return true;
  }

  return false;
}
