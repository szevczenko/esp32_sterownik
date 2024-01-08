#include "measure.h"

#include <stdint.h>

#include "cmd_server.h"
#include "config.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/timers.h"
#include "measure.h"
#include "menu_param.h"
#include "parse_cmd.h"
#include "ultrasonar.h"

#define MODULE_NAME "[Meas] "
#define DEBUG_LVL   PRINT_WARNING

#if CONFIG_DEBUG_MEASURE
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

static const adc_bitwidth_t width = ADC_BITWIDTH_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;

#define ADC_IN_CH    ADC_CHANNEL_6
#define ADC_MOTOR_CH ADC_CHANNEL_7
#define ADC_SERVO_CH ADC_CHANNEL_4
#define ADC_12V_CH   ADC_CHANNEL_5
#define ADC_CE_CH    ADC_CHANNEL_0

#ifndef ADC_REFRES
#define ADC_REFRES 4096
#endif

#define DEFAULT_VREF  1100    // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES 64    // Multisampling

#define DEFAULT_MOTOR_CALIBRATION_VALUE 1830
#define SILOS_START_MEASURE             100

typedef struct
{
  char* ch_name;
  adc_channel_t channel;
  adc_unit_t unit;
  uint32_t adc;
  uint32_t filtered_adc;
  uint32_t filter_table[FILTER_TABLE_SIZE];
  float meas_voltage;
} meas_data_t;

static meas_data_t meas_data[MEAS_CH_LAST] =
  {
    [MEAS_CH_IN] = {.unit = ADC_UNIT_1,  .channel = ADC_IN_CH,     .ch_name = "MEAS_CH_IN"         },
    [MEAS_CH_MOTOR] = { .unit = ADC_UNIT_1, .channel = ADC_MOTOR_CH,  .ch_name = "MEAS_CH_MOTOR"      },
    [MEAS_CH_12V] = { .unit = ADC_UNIT_1, .channel = ADC_12V_CH,    .ch_name = "MEAS_CH_12V"        },
#if CONFIG_DEVICE_SIEWNIK
    [MEAS_CH_SERVO] = { .unit = ADC_UNIT_1, .channel = ADC_SERVO_CH,  .ch_name = "MEAS_CH_SERVO"      },
    [MEAS_CH_TEMP] = { .unit = ADC_UNIT_1, .channel = ADC_CE_CH,     .ch_name = "MEAS_CH_TEMP"       },
    [MEAS_CH_CHECK_SERVO] = { .unit = ADC_UNIT_2, .channel = ADC_CHANNEL_4, .ch_name = "MEAS_CH_CHECK_SERVO"},
#endif

#if CONFIG_DEVICE_SOLARKA
    [MEAS_CH_TEMP] = { .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_4, .ch_name = "MEAS_CH_TEMP"       },
    [MEAS_CH_CHECK_VIBRO] = { .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_0, .ch_name = "MEAS_CH_CHECK_VIBRO"},
    [MEAS_CH_CHECK_MOTOR] = { .unit = ADC_UNIT_1, .channel = ADC_CHANNEL_3, .ch_name = "MEAS_CH_CHECK_MOTOR"},
#endif
};

static uint32_t table_size;
static uint32_t table_iter;
uint32_t motor_calibration_meas;
// #if CONFIG_DEVICE_SOLARKA
static TimerHandle_t motorCalibrationTimer;
// #endif

#if CONFIG_DEVICE_SIEWNIK
static TimerHandle_t servoCalibrationTimer;
static uint32_t servo_calibration_value;

static void measure_get_servo_calibration( TimerHandle_t xTimer )
{
  servo_calibration_value = measure_get_filtered_value( MEAS_CH_SERVO );
  LOG( PRINT_INFO, "MEASURE SERVO Calibration value = %d", servo_calibration_value );
}

#endif

static void measure_get_motor_calibration( TimerHandle_t xTimer )
{
  if ( !menuGetValue( MENU_MOTOR_IS_ON ) )
  {
    motor_calibration_meas = measure_get_filtered_value( MEAS_CH_MOTOR );
    LOG( PRINT_INFO, "MEASURE MOTOR Calibration value = %d", motor_calibration_meas );
  }
  else
  {
    LOG( PRINT_INFO, "MEASURE MOTOR Fail get. Motor is on" );
  }
}

static uint32_t filtered_value( uint32_t* tab, uint8_t size )
{
  uint16_t ret_val = *tab;

  for ( uint8_t i = 1; i < size; i++ )
  {
    ret_val = ( ret_val + tab[i] ) / 2;
  }

  return ret_val;
}

void init_measure( void )
{
  motor_calibration_meas = DEFAULT_MOTOR_CALIBRATION_VALUE;
#if CONFIG_DEVICE_SIEWNIK
  servo_calibration_value = 2300;
#endif
}

static void _read_adc_values( adc_oneshot_unit_handle_t adc1_handle, adc_oneshot_unit_handle_t adc2_handle )
{
  for ( uint8_t ch = 0; ch < MEAS_CH_LAST; ch++ )
  {
    meas_data[ch].adc = 0;
    // Multisampling
    for ( int i = 0; i < NO_OF_SAMPLES; i++ )
    {
      int adc_reading = 0;
      ESP_ERROR_CHECK( adc_oneshot_read( meas_data[ch].unit == ADC_UNIT_1 ? adc1_handle : adc2_handle, meas_data[ch].channel, &adc_reading ) );
      LOG( PRINT_DEBUG, "ADC%d Channel[%d] Raw Data: %d", meas_data[ch].unit + 1, meas_data[ch].channel, adc_reading );
      meas_data[ch].adc += adc_reading;
    }

    meas_data[ch].adc /= NO_OF_SAMPLES;
    meas_data[ch].filter_table[table_iter % FILTER_TABLE_SIZE] = meas_data[ch].adc;
    meas_data[ch].filtered_adc = filtered_value( &meas_data[ch].adc, table_size );
  }

  table_iter++;
  if ( table_size < FILTER_TABLE_SIZE - 1 )
  {
    table_size++;
  }
}

static void measure_process( void* arg )
{
  (void) arg;
  //-------------ADC1 Init---------------//
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
  };
  ESP_ERROR_CHECK( adc_oneshot_new_unit( &init_config1, &adc1_handle ) );

  //-------------ADC1 Config---------------//
  adc_oneshot_chan_cfg_t config = {
    .bitwidth = width,
    .atten = atten,
  };

  for ( int i = 0; i < MEAS_CH_LAST; i++ )
  {
    if ( meas_data[i].unit == ADC_UNIT_1 )
    {
      ESP_ERROR_CHECK( adc_oneshot_config_channel( adc1_handle, meas_data[i].channel, &config ) );
    }
  }

  //-------------ADC2 Init---------------//
  adc_oneshot_unit_handle_t adc2_handle;
  adc_oneshot_unit_init_cfg_t init_config2 = {
    .unit_id = ADC_UNIT_2,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ESP_ERROR_CHECK( adc_oneshot_new_unit( &init_config2, &adc2_handle ) );

  //-------------ADC2 Config---------------//
  for ( int i = 0; i < MEAS_CH_LAST; i++ )
  {
    if ( meas_data[i].unit == ADC_UNIT_2 )
    {
      ESP_ERROR_CHECK( adc_oneshot_config_channel( adc2_handle, meas_data[i].channel, &config ) );
    }
  }

  while ( 1 )
  {
    vTaskDelay( MS2ST( 100 ) );

    _read_adc_values( adc1_handle, adc2_handle );

    // LOG(PRINT_INFO, "%s %d", meas_data[MEAS_CH_CHECK_VIBRO].ch_name, meas_data[MEAS_CH_CHECK_VIBRO].filtered_adc);
    // LOG(PRINT_INFO, "%s %d", meas_data[MEAS_CH_CHECK_MOTOR].ch_name, meas_data[MEAS_CH_CHECK_MOTOR].filtered_adc);

    if ( ultrasonar_is_connected() )
    {
      uint32_t silos_height = menuGetValue( MENU_SILOS_HEIGHT ) * 10;
      uint32_t silos_distance = ultrasonar_get_distance() > SILOS_START_MEASURE ? ultrasonar_get_distance() - SILOS_START_MEASURE : 0;
      if ( silos_distance > silos_height )
      {
        silos_distance = silos_height;
      }

      int silos_percent = ( silos_height - silos_distance ) * 100 / silos_height;
      if ( ( silos_percent < 0 ) || ( silos_percent > 100 ) )
      {
        silos_percent = 0;
      }
      uint32_t silos_is_low = silos_percent < 10;
      LOG( PRINT_INFO, "Silos %d %d", silos_percent, silos_is_low );
      menuSetValue( MENU_LOW_LEVEL_SILOS, silos_is_low );
      menuSetValue( MENU_SILOS_LEVEL, (uint32_t) silos_percent );
      menuSetValue( MENU_SILOS_SENSOR_IS_CONECTED, 1 );
    }
    else
    {
      menuSetValue( MENU_SILOS_SENSOR_IS_CONECTED, 0 );
      menuSetValue( MENU_LOW_LEVEL_SILOS, 0 );
      menuSetValue( MENU_SILOS_LEVEL, 0 );
    }

    menuSetValue( MENU_VOLTAGE_ACCUM, (uint32_t) ( accum_get_voltage() * 10000.0 ) );
    menuSetValue( MENU_CURRENT_MOTOR, (uint32_t) ( measure_get_current( MEAS_CH_MOTOR, 0.1 ) ) );
    menuSetValue( MENU_TEMPERATURE, (uint32_t) ( measure_get_temperature() ) );
    menuSetValue( MENU_VOLTAGE_SERVO, (uint32_t) ( measure_get_servo_voltage() * 1000.0 ) );
    /* DEBUG */
    // menuPrintParameter(MENU_VOLTAGE_ACCUM);
    // menuPrintParameter(MENU_CURRENT_MOTOR);
    // menuPrintParameter(MENU_TEMPERATURE);
  }
}

void measure_start( void )
{
  xTaskCreate( measure_process, "measure_process", 4096, NULL, 10, NULL );
#if CONFIG_DEVICE_SIEWNIK
  servoCalibrationTimer = xTimerCreate( "servoCalibrationTimer", MS2ST( 1000 ), pdFALSE, (void*) 0,
                                        measure_get_servo_calibration );
#endif

  motorCalibrationTimer = xTimerCreate( "motorCalibrationTimer", MS2ST( 1000 ), pdFALSE, (void*) 0,
                                        measure_get_motor_calibration );

  init_measure();
}

void measure_meas_calibration_value( void )
{
  LOG( PRINT_INFO, "%s", __func__ );
#if CONFIG_DEVICE_SIEWNIK
  xTimerStart( servoCalibrationTimer, 0 );
#endif
  xTimerStart( motorCalibrationTimer, 0 );
}

uint32_t measure_get_filtered_value( enum_meas_ch type )
{
  if ( type < MEAS_CH_LAST )
  {
    return meas_data[type].filtered_adc;
  }

  return 0;
}

float measure_get_temperature( void )
{
#if CONFIG_DEVICE_SIEWNIK
  int temp = -( (int) measure_get_filtered_value( MEAS_CH_TEMP ) ) / 31 + 100;
  if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2100 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 2300 ) )
  {
    temp = temp / 1.1;
  }
  else if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2300 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 2400 ) )
  {
    temp = temp;
  }
  else if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2400 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 2550 ) )
  {
    temp = temp * 1.1;
  }
  else if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2550 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 2570 ) )
  {
    temp = temp * 1.2;
  }
  else if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2570 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 2600 ) )
  {
    temp = temp * 1.3;
  }
  else if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2600 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 2650 ) )
  {
    temp = temp * 1.4;
  }
  else if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2650 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 2800 ) )
  {
    temp = temp * 1.6;
  }
  else if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2800 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 2820 ) )
  {
    temp = temp * 1.9;
  }
  else if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2820 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 2900 ) )
  {
    temp = temp * 2.1;
  }
  else if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 2900 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 3000 ) )
  {
    temp = temp * 2.5;
  }

  if ( ( measure_get_filtered_value( MEAS_CH_TEMP ) >= 3000 ) && ( measure_get_filtered_value( MEAS_CH_TEMP ) <= 4000 ) )
  {
    temp = 0;
  }

  LOG( PRINT_DEBUG, "Temperature %d %d", measure_get_filtered_value( MEAS_CH_TEMP ), temp );
  return temp;
#endif

#if CONFIG_DEVICE_SOLARKA
  int tempx = ( (int) measure_get_filtered_value( MEAS_CH_TEMP ) ) / 25 - 26;
  int temp = 0.0017 * ( tempx * tempx ) + 0.5 * tempx + 3;

  /* if((measure_get_filtered_value(MEAS_CH_TEMP)>=1000)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=1200)){
    temp=temp/1.1;
     }else if((measure_get_filtered_value(MEAS_CH_TEMP)>=1200)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=1400)){
    temp=temp/1.3;
    }else  if((measure_get_filtered_value(MEAS_CH_TEMP)>=1400)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=1550)){
    temp=temp/1.5;
    }else  if((measure_get_filtered_value(MEAS_CH_TEMP)>=1550)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=1750)){
    temp=temp/1.6;
    }else  if((measure_get_filtered_value(MEAS_CH_TEMP)>=1750)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=1900)){
    temp=temp/1.7;
    }else  if((measure_get_filtered_value(MEAS_CH_TEMP)>=1900)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=2000)){
    temp=temp/1.5;
    }else if((measure_get_filtered_value(MEAS_CH_TEMP)>=1900)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=2050)){
    temp=temp/1.4;
    }else if((measure_get_filtered_value(MEAS_CH_TEMP)>=2050)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=2250)){
    temp=temp/1.35;
    }else if((measure_get_filtered_value(MEAS_CH_TEMP)>=2250)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=2350)){
    temp=temp/1.30;
    }else if((measure_get_filtered_value(MEAS_CH_TEMP)>=2350)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=2450)){
    temp=temp/1.2;
    }else if((measure_get_filtered_value(MEAS_CH_TEMP)>=2450)&&(measure_get_filtered_value(MEAS_CH_TEMP)<=3050)){
    temp=temp/1.1;}
    */

  LOG( PRINT_DEBUG, "Temperature %d %d", measure_get_filtered_value( MEAS_CH_TEMP ), temp );
  return temp;
#endif
}

float measure_get_servo_voltage( void )
{
#if CONFIG_DEVICE_SIEWNIK
  uint32_t adc = measure_get_filtered_value( MEAS_CH_SERVO ) > servo_calibration_value ? 0 : servo_calibration_value - measure_get_filtered_value( MEAS_CH_SERVO );
  float voltage = (float) adc * 5 / servo_calibration_value /* mAmp */;
  LOG( PRINT_DEBUG, "Servo: Adc %d calib %d voltage %f", adc, servo_calibration_value, voltage );
  return voltage;
#else
  return 0;
#endif
}

float measure_get_current( enum_meas_ch type, float resistor )
{
#if CONFIG_DEVICE_SIEWNIK
  LOG( PRINT_DEBUG, "Adc %d calib %d", measure_get_filtered_value( type ), motor_calibration_meas );
  uint32_t adc = measure_get_filtered_value( type ) < motor_calibration_meas ? 0 : measure_get_filtered_value( type ) - motor_calibration_meas;
  float current = (float) adc * 10 /* mAmp */;
  LOG( PRINT_DEBUG, "Adc %d calib %d curr %f", adc, motor_calibration_meas, current );
#endif

#if CONFIG_DEVICE_SOLARKA
  LOG( PRINT_DEBUG, "Adc %d calib %d", measure_get_filtered_value( type ), motor_calibration_meas );
  uint32_t adc = measure_get_filtered_value( type ) < motor_calibration_meas ? 0 : measure_get_filtered_value( type ) - motor_calibration_meas;

  float voltage = menuGetValue( MENU_VOLTAGE_ACCUM ) / 100.0;
  float correction = ( 14.2 - voltage ) * 100;
  float current_meas = (float) adc * 0.92;
  float current = current_meas /* + correction*/ /* Amp */;
  LOG( PRINT_DEBUG, "voltage %f correction %f current_meas %f current %f", voltage, correction, current_meas, current );
  LOG( PRINT_DEBUG, "Adc %d calib %d curr %f", adc, motor_calibration_meas, current );
#endif

  return current;
}

float accum_get_voltage( void )
{
  float voltage = 0;

#if CONFIG_DEVICE_SOLARKA
  voltage = (float) measure_get_filtered_value( MEAS_CH_12V ) / 4096.0 / 2.5;
#else
  voltage = (float) measure_get_filtered_value( MEAS_CH_12V ) / 4096.0 / 2.6;
#endif
  return voltage;
}
