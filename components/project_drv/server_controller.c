#include "server_controller.h"

#include <math.h>
#include <stdbool.h>

#include "cmd_server.h"
#include "e108_position_driver.h"
#include "error_siewnik.h"
#include "error_solarka.h"
#include "http_server.h"
#include "measure.h"
#include "motor.h"
#include "parameters.h"
#include "parse_cmd.h"
#include "pwm_drv.h"
#include "servo.h"
#include "tank_sensor.h"
#include "vibro.h"
#include "wifidrv.h"

#define MODULE_NAME "[Srvr Ctrl] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_SERVER_CONTROLLER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define SYSTEM_ON_PIN  15
#define MOTOR_PWM_PIN  27
#define VIBRO_PWM_PIN  25
#define SERVO_PWM_PIN  26
#define MOTOR_PWM_PIN2 25
#define max_rpm        3000.0

#define PID_ENABLED 0

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
  uint8_t servo_value_after_correction;
  uint8_t servo_new_value;
  uint8_t servo_set_value;
  uint32_t servo_set_timer;
  uint8_t motor_value;
  uint32_t motor_rpm;
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

  uint32_t kg_per_ha;
  float velocity;
  uint32_t velocity_set;
  e108_gnss_status_t velocity_sensor_status;
  float machine_height;
  bool auto_mode;

  // New fields for auto mode
  float working_width_m;
  int32_t correction_factor;
  uint32_t servo_open_delay_s;
  uint32_t seeding_start_speed_kmh;
  bool seeding_active;
  uint32_t seeding_start_time;

#if PID_ENABLED
  uint32_t density;    // Remove if no needed
  // PID controller variables
  float pid_kp;    // Proportional gain
  float pid_ki;    // Integral gain
  float pid_kd;    // Derivative gain
  float pid_error_sum;    // Integral term accumulator
  float pid_last_error;    // Last error for derivative term
  float pid_target_flow_rate;    // Target flow rate in L/min
  float pid_last_output;    // Last PID output
  uint32_t pid_last_time;    // Last time PID was calculated
#endif

  pwm_drv_t motor1_pwm;
  pwm_drv_t motor2_pwm;
  pwm_drv_t servo_pwm_drv;

  // New fields for system shutdown delay
  bool system_shutdown_pending;
  uint32_t system_shutdown_time;
} server_controller_ctx;

static server_controller_ctx ctx;
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

static float interpolate_pwm_from_rpm( float rpm, float voltage, float current )
{
  // Table data
  const float rpm_table[] = { 200, 440, 600, 1150, 1420, 1740, 2000, 2250, 2500, 2700, 3000 };
  const float pwm_table[] = { 1, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
  const float current_table[] = { 0.3, 0.3, 0.7, 0.9, 1.4, 1.7, 2.2, 2.7, 3.9, 4.6, 5.5 };
  const float voltage_table[] = { 14.4, 14.4, 14.4, 14.4, 14.4, 14.4, 14.4, 14.4, 14.4, 14.4, 14.4 };
  const int table_size = sizeof( rpm_table ) / sizeof( rpm_table[0] );

  // Clamp RPM to the table range
  if ( rpm <= rpm_table[0] )
  {
    return pwm_table[0];
  }
  if ( rpm >= rpm_table[table_size - 1] )
  {
    return pwm_table[table_size - 1];
  }

  // Find the two closest points in the table
  for ( int i = 0; i < table_size - 1; i++ )
  {
    if ( rpm >= rpm_table[i] && rpm <= rpm_table[i + 1] )
    {
      // Perform linear interpolation for PWM
      float t = ( rpm - rpm_table[i] ) / ( rpm_table[i + 1] - rpm_table[i] );
      float interpolated_pwm = pwm_table[i] + t * ( pwm_table[i + 1] - pwm_table[i] );

      // Adjust based on voltage and current
      float avg_voltage = ( voltage_table[i] + voltage_table[i + 1] ) / 2.0;
      float avg_current = ( current_table[i] + current_table[i + 1] ) / 2.0;

      if ( voltage < avg_voltage )
      {
        interpolated_pwm *= ( voltage / avg_voltage );    // Scale down if voltage is lower
      }
      if ( current > avg_current )
      {
        interpolated_pwm *= ( avg_current / current );    // Scale down if current is higher
      }

      return interpolated_pwm;
    }
  }

  return 0;    // Fallback (should not reach here)
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
  static bool prev_system_on = false;

  // Check for transition from ON to OFF
  if ( prev_system_on && !ctx.system_on )
  {
    // System is being turned off, start the shutdown delay
    ctx.system_shutdown_pending = true;
    ctx.system_shutdown_time = xTaskGetTickCount() + MS2ST( 1500 );    // 1.5 second delay
    LOG( PRINT_INFO, "System shutdown initiated with 1.5s delay" );
  }

  // Update previous state for next call
  prev_system_on = ctx.system_on;

  // Determine actual GPIO state based on shutdown timer
  bool actual_system_state = ctx.system_on;
  if ( ctx.system_shutdown_pending )
  {
    if ( xTaskGetTickCount() >= ctx.system_shutdown_time )
    {
      // Delay expired, complete the shutdown
      ctx.system_shutdown_pending = false;
      actual_system_state = false;
      LOG( PRINT_INFO, "System shutdown completed after delay" );
    }
    else
    {
      // Still in delay period, keep system on
      actual_system_state = true;
    }
  }

  // Set the actual GPIO pin state
  gpio_set_level( SYSTEM_ON_PIN, actual_system_state ? 1 : 0 );

  LOG( PRINT_DEBUG, "motor %d %f %d", ctx.motor_on, ctx.motor_pwm, ctx.motor_value );
  if ( ctx.motor_on )
  {
    float duty = (float) ctx.motor_pwm;
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

#if PID_ENABLED
// PID controller implementation
static float calculate_pid( float target, float actual, uint32_t current_time_ms )
{
  float error = target - actual;
  float dt = ( current_time_ms - ctx.pid_last_time ) / 1000.0f;    // Convert to seconds

  if ( dt <= 0.0f || dt > 1.0f )
  {
    // Time interval too small or too large (e.g., first run)
    ctx.pid_last_time = current_time_ms;
    ctx.pid_last_error = error;
    ctx.pid_error_sum = 0;
    return ctx.pid_last_output;
  }

  // Calculate proportional term
  float p_term = ctx.pid_kp * error;

  // Calculate integral term with anti-windup
  ctx.pid_error_sum += error * dt;
  // Limit integral term to prevent windup
  if ( ctx.pid_error_sum > 100.0f )
    ctx.pid_error_sum = 100.0f;
  if ( ctx.pid_error_sum < -100.0f )
    ctx.pid_error_sum = -100.0f;
  float i_term = ctx.pid_ki * ctx.pid_error_sum;

  // Calculate derivative term
  float d_term = ctx.pid_kd * ( error - ctx.pid_last_error ) / dt;
  ctx.pid_last_error = error;

  // Calculate PID output
  float output = p_term + i_term + d_term;

  // Limit output to 0-100 range for servo value
  if ( output > 100.0f )
    output = 100.0f;
  if ( output < 0.0f )
    output = 0.0f;

  ctx.pid_last_output = output;
  ctx.pid_last_time = current_time_ms;

  LOG( PRINT_DEBUG, "PID: target=%.2f actual=%.2f error=%.2f p=%.2f i=%.2f d=%.2f out=%.2f",
       target, actual, error, p_term, i_term, d_term, output );

  return output;
}

// Calculate target flow rate based on kg_per_ha, velocity and working width
static float calculate_target_flow_rate( uint32_t kg_per_ha, float velocity_kmh, float working_width_m, uint32_t density_kgm3 )
{
  // Formula: flow_rate(L/min) = kg_per_ha * velocity_kmh * working_width_m * (10/60) / density_kgm3
  // 10/60 factor:
  // - 10 converts ha (10000 m²) to m² and km to m
  // - 60 converts km/h to km/min

  float flow_rate_lpm = (float) kg_per_ha * velocity_kmh * working_width_m * ( 10.0f / 60.0f ) / (float) density_kgm3;
  LOG( PRINT_DEBUG, "Target flow rate: %.2f L/min (kg/ha=%lu, v=%.2f, w=%.2f, d=%lu)",
       flow_rate_lpm, kg_per_ha, velocity_kmh, working_width_m, density_kgm3 );

  return flow_rate_lpm;
}
#endif

static double _convert_kg_per_s_to_servo_value( uint32_t grain_size, double kg_per_s )
{
  if ( grain_size < 1 || grain_size > 10 )
  {
    return 0.0;    // Unsupported grain size
  }

  // Dynamically generate coefficients based on grain size
  double a = 18.3 + ( grain_size - 1 ) * 0.1;    // Base offset increases slightly with grain size
  double b = 1598.0 + ( grain_size - 1 ) * 479.0;    // Linear term increases with grain size
  double c = -19865.0 - ( grain_size - 1 ) * 13708.0;    // Quadratic term decreases with grain size
  double d = 113184.0 + ( grain_size - 1 ) * 150402.0;    // Cubic term increases with grain size

  // Calculate servo value using the polynomial
  return a + b * kg_per_s + c * pow( kg_per_s, 2 ) + d * pow( kg_per_s, 3 );
}

static void state_init( void )
{
  gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = ( 1 << SYSTEM_ON_PIN ) | ( 1 << VIBRO_PWM_PIN ),
    .pull_down_en = 0,
    .pull_up_en = 0 };
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

#if PID_ENABLED
  // Initialize PID controller parameters
  ctx.pid_kp = 2.0f;    // Initial proportional gain
  ctx.pid_ki = 0.5f;    // Initial integral gain
  ctx.pid_kd = 0.1f;    // Initial derivative gain
  ctx.pid_error_sum = 0.0f;
  ctx.pid_last_error = 0.0f;
  ctx.pid_last_output = 0.0f;
  ctx.pid_last_time = 0;
#endif

  change_state( STATE_IDLE );
}

static void state_idle( void )
{
  ctx.servo_value = 0;
  ctx.motor_value = 0;
  ctx.motor_on = false;
  ctx.servo_on = false;
  ctx.system_on = 0;

  ctx.working_state_req = (bool) parameters_getValue( PARAM_START_SYSTEM );
  ctx.emergency_disable = (bool) parameters_getValue( PARAM_EMERGENCY_DISABLE );
  vibro_stop();
  parameters_setValue( PARAM_MOTOR_IS_ON, 0 );
  parameters_setValue( PARAM_SERVO_IS_ON, 0 );

  if ( ctx.emergency_disable )
  {
    change_state( STATE_EMERGENCY_DISABLE );
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

static void _manual_working( void )
{
  ctx.system_on = (bool) parameters_getValue( PARAM_START_SYSTEM );
  ctx.servo_value = (uint8_t) parameters_getValue( PARAM_SERVO );
  ctx.motor_value = (uint8_t) parameters_getValue( PARAM_MOTOR );
  ctx.motor_on = (uint8_t) parameters_getValue( PARAM_MOTOR_IS_ON );
  ctx.servo_on = parameters_getValue( PARAM_SERVO_IS_ON ) > 0;

#if CONFIG_DEVICE_SOLARKA
  vibro_config( parameters_getValue( PARAM_PERIOD ) * 1000, parameters_getValue( PARAM_SERVO ) );
  if ( parameters_getValue( PARAM_SERVO_IS_ON ) )
  {
    vibro_start();
  }
  else
  {
    vibro_stop();
  }
#endif
}

#if PID_ENABLED
static uint32_t _size_of_grain_to_density( uint32_t size_of_grain )
{
  switch ( size_of_grain )
  {
    case 0:
      return 1000;    // kg/m^3

    case 1:
      return 800;    // kg/m^3

    case 2:
      return 600;    // kg/m^3

    default:
      return 1000;    // kg/m^3
  }
}
#endif

uint32_t _minimal_servo_open( uint32_t size_of_grain )
{
  const uint32_t min_servo_open_array[] = { 14, 15, 17, 18, 19, 20, 22, 23, 24, 25 };
  if ( size_of_grain < 1 || size_of_grain > 10 )
  {
    return 0;
  }

  return (uint32_t) min_servo_open_array[size_of_grain - 1];
}

static void _auto_working( void )
{
  // Read position sensor data
  ctx.velocity_sensor_status = e108_get_status();
  parameters_setValue( PARAM_VELOCITY_SENSOR_STATUS, ctx.velocity_sensor_status );
  e108_position_info_t position = { 0 };
  e108_get_position( &position );
  ctx.velocity = position.filtered_speed_kmh;
  parameters_setValue( PARAM_VELOCITY_HMS, (uint32_t) ( ctx.velocity * 10.0f ) );
  if ( parameters_getValue( PARAM_RESET_DISTANCE ) == 1 )
  {
    e108_reset_distance();
    parameters_setValue( PARAM_RESET_DISTANCE, 0 );
  }
  parameters_setValue( PARAM_DISTANCE_HM, position.distance_km * 10 );

  // Read basic parameters
  ctx.motor_on = parameters_getValue( PARAM_MOTOR_IS_ON );
  ctx.kg_per_ha = parameters_getValue( PARAM_GRAIN_PER_HECTARE );
  ctx.velocity_set = parameters_getValue( PARAM_SET_VELOCITY_KM_H );
  ctx.motor_rpm = parameters_getValue( PARAM_MOTOR_RPM_PER_100 ) * 100;

  // Read auto mode parameters
  ctx.machine_height = (float) parameters_getValue( PARAM_HIGH_OF_MACHINE_CM ) / 100.0f;    // Convert cm to m
  ctx.working_width_m = (float) parameters_getValue( PARAM_WORKING_WIDTH_СM ) / 100.0f;    // Convert сm to m
  ctx.correction_factor = (int32_t) parameters_getValue( PARAM_CORRECTION_FACTOR ) - 100;
  ctx.servo_open_delay_s = parameters_getValue( PARAM_SERVO_OPEN_DELAY_S );
  ctx.seeding_start_speed_kmh = parameters_getValue( PARAM_SEEDING_START_SPEED_KMH );

  //Implement velocity sensor
  if ( ctx.velocity_sensor_status != E108_READY )
  {
    ctx.velocity = ctx.velocity_set;
  }

  // LOG( PRINT_INFO, "Auto mode parameters:" );
  // LOG( PRINT_INFO, "Velocity = %lu, Start speed = %lu", ctx.velocity, ctx.seeding_start_speed_kmh / 10 );
  // LOG( PRINT_INFO, "Working width = %.1f m", ctx.working_width_m );
  // LOG( PRINT_INFO, "Machine height = %.2f m", ctx.machine_height );
  // LOG( PRINT_INFO, "Correction factor = %ld%%", ctx.correction_factor );
  // LOG( PRINT_INFO, "Servo delay = %lu s", ctx.servo_open_delay_s );
  // LOG( PRINT_INFO, "Grain per hectare = %lu", ctx.kg_per_ha );

  // Determine if seeding should start based on speed
  if ( ctx.velocity >= ctx.seeding_start_speed_kmh )
  {
    // Vehicle is moving faster than start speed
    if ( !ctx.seeding_active )
    {
      // Start seeding with delay
      ctx.seeding_active = true;
      ctx.seeding_start_time = xTaskGetTickCount() + MS2ST( ctx.servo_open_delay_s * 100 );    // Convert deciseconds to ms
      LOG( PRINT_DEBUG, "Seeding start initiated with delay %lu s", ctx.servo_open_delay_s );
    }
  }
  else
  {
    // Vehicle is moving too slow, stop seeding
    ctx.seeding_active = false;
    ctx.servo_on = false;
    ctx.servo_value = 0;
    LOG( PRINT_DEBUG, "Speed too low, seeding stopped" );
    return;
  }

  // Check if we're in the delay period
  if ( ctx.seeding_active && xTaskGetTickCount() < ctx.seeding_start_time )
  {
    // Still in delay period, don't open servo yet
    ctx.servo_on = false;
    ctx.servo_value = 0;
    LOG( PRINT_DEBUG, "In delay period, waiting to start seeding" );
    return;
  }

  // Activate servo if seeding is active and delay period has passed
  if ( ctx.seeding_active )
  {
    ctx.servo_on = ctx.motor_rpm > 0 ? ctx.motor_on : false;
  }
  else
  {
    ctx.servo_on = false;
  }

  parameters_setValue( PARAM_SEEDING_IS_ACTIVE, ctx.servo_on );

  // Calculate seeding parameters
  uint32_t size_of_grain = parameters_getValue( PARAM_SIZE_OF_GRAIN );

  // Option 1: Use the configured working width
  double working_width = ctx.working_width_m;

#if 0
  // Option 2: Calculate working width based on physics if sensor is connected
  if ( ctx.velocity_sensor_status == E108_READY )
  {
    // double motor_rpm = max_rpm / 100.0 * ctx.motor_value;
    double _R = 0.3;    // Example value. 30 [cm]
    double grain_throwing_speed = (double) ctx.motor_rpm * 2 * 3.14159265359 * _R / 60.0;
    working_width = grain_throwing_speed * sqrt( 2 * ctx.machine_height / 9.81 );
  }
#endif
  LOG( PRINT_DEBUG, "working width = %.2f m", working_width );

  // Check if tank sensor is connected and PID is enabled
#if PID_ENABLED
  if ( tank_sensor_is_connected() )
  {
    // Get material density based on grain size
    ctx.density = _size_of_grain_to_density( size_of_grain );

    // Calculate target flow rate based on kg_per_ha, velocity, and working width
    ctx.pid_target_flow_rate = calculate_target_flow_rate( ctx.kg_per_ha, ctx.velocity, working_width, ctx.density );

    // Get actual flow rate from tank sensor
    float actual_flow_rate = tank_sensor_get_flow_rate();

    // Calculate PID output (servo value)
    uint32_t current_time = xTaskGetTickCount();
    float servo_value_float = calculate_pid( ctx.pid_target_flow_rate, actual_flow_rate, current_time );

    // Apply correction factor (-100% to +100%)
    double correction_multiplier = 1.0 + ( (double) ctx.correction_factor / 100.0 );
    servo_value_float *= correction_multiplier;

    // Clamp servo value to valid range
    if ( servo_value_float < 0 )
      servo_value_float = 0;
    if ( servo_value_float > 100 )
      servo_value_float = 100;

    // Convert to integer
    uint8_t servo_value = (uint8_t) servo_value_float;

    LOG( PRINT_DEBUG, "PID Servo control: target=%.2f actual=%.2f servo=%u",
         ctx.pid_target_flow_rate, actual_flow_rate, servo_value );

    ctx.servo_value = servo_value;
  }
  else
#endif
  {
    // Convert velocity from km/h to m/s
    float velocity_m_s = ctx.velocity / 3.6f;

    // Convert kg_per_ha to kg/m²
    float kg_per_m2 = (float) ctx.kg_per_ha / 10000.0f;

    // Fallback to calculation using kg_per_s and grain size
    double kg_per_s = (double) kg_per_m2 * (double) velocity_m_s * ctx.working_width_m;
    double servo_value = _convert_kg_per_s_to_servo_value( size_of_grain, kg_per_s );

    // Clamp servo value to valid range
    if ( servo_value < 0 )
    {
      servo_value = 0;
    }
    if ( servo_value > 100 )
    {
      servo_value = 100;
    }

    ctx.servo_value = (uint8_t) servo_value;
  }

  uint32_t minimal_servo_open = _minimal_servo_open( size_of_grain );
  //(uint32_t) ( (double) parameters_getValue( PARAM_SERVO_MINIMAL_OPEN ) + 1000.0 / (double) ctx.density * (double) parameters_getValue( PARAM_SERVO_MINIMAL_OPEN_CORRECTION ) / 100.0 );
  ctx.servo_value_after_correction = ctx.servo_value < minimal_servo_open ? minimal_servo_open : ctx.servo_value;

  // Convert motor RPM to PWM duty cycle using interpolation
  float voltage = parameters_getValue( PARAM_VOLTAGE_ACCUM ) / 100.0f;    // Convert cV to V
  float current = parameters_getValue( PARAM_CURRENT_MOTOR ) / 100.0f;    // Convert cA to A
  ctx.motor_value = interpolate_pwm_from_rpm( ctx.motor_rpm, voltage, current );

  LOG( PRINT_DEBUG, "Speed = %f, RPM = %f, Servo = %f, Motor = %f Current = %f, Voltage = %f",
       ctx.velocity, ctx.motor_rpm, ctx.servo_value_after_correction, ctx.motor_value, current, voltage );
  LOG( PRINT_DEBUG, "DISTANCE %f", position.distance_km );

  // Set value after correction
  parameters_setValue( PARAM_SERVO, ctx.servo_value );
  ctx.servo_value = ctx.servo_value_after_correction;
  LOG( PRINT_DEBUG, "Servo value = %u", ctx.servo_value_after_correction );
}

static void state_working( void )
{
  ctx.working_state_req = (bool) parameters_getValue( PARAM_START_SYSTEM );
  ctx.emergency_disable = (bool) parameters_getValue( PARAM_EMERGENCY_DISABLE );
  ctx.servo_open_calibration_req = (bool) parameters_getValue( PARAM_OPEN_SERVO_REGULATION_FLAG );
  ctx.servo_close_calibration_req = (bool) parameters_getValue( PARAM_CLOSE_SERVO_REGULATION_FLAG );
  ctx.auto_mode = (bool) parameters_getValue( PARAM_AUTO_MODE );

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

  if ( ctx.auto_mode )
  {
    _auto_working();
  }
  else
  {
    _manual_working();
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

static void state_low_voltage( void )
{
  ctx.servo_value = 0;
  ctx.motor_value = 0;
  ctx.motor_on = false;
  ctx.servo_on = false;
  float voltage = accum_get_voltage();
  if ( 5 < voltage )
  {
    change_state( STATE_IDLE );
  }
}

static void _task( void* arg )
{
  parameters_setValue( PARAM_CLOSE_SERVO_REGULATION_FLAG, 0 );
  parameters_setValue( PARAM_OPEN_SERVO_REGULATION_FLAG, 0 );
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
        state_low_voltage();
        break;

      default:
        change_state( STATE_IDLE );
        break;
    }
    float voltage = accum_get_voltage();
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