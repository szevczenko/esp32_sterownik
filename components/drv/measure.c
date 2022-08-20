#include <stdint.h>
#include "measure.h"
#include "config.h"
#include "freertos/timers.h"
#include "menu_param.h"
#include "parse_cmd.h"
#include "cmd_server.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "measure.h"
#include "ultrasonar.h"

#define MODULE_NAME    "[Meas] "
#define DEBUG_LVL      PRINT_INFO

#if CONFIG_DEBUG_MEASURE
#define LOG(_lvl, ...) \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

static adc_bits_width_t width = ADC_WIDTH_BIT_12;
static adc_atten_t atten = ADC_ATTEN_DB_11;

#define ADC_IN_CH                          ADC_CHANNEL_6
#define ADC_MOTOR_CH                       ADC_CHANNEL_7
#define ADC_SERVO_CH                       ADC_CHANNEL_4
#define ADC_12V_CH                         ADC_CHANNEL_5
#define ADC_CE_CH                          ADC_CHANNEL_0

#ifndef ADC_REFRES
#define ADC_REFRES                         4096
#endif

#define DEFAULT_VREF                       1100 //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES                      64   //Multisampling

#define DEFAULT_MOTOR_CALIBRATION_VALUE    1830

typedef struct
{
    char *ch_name;
    adc_channel_t channel;
    adc_unit_t unit;
    uint32_t adc;
    uint32_t filtered_adc;
    uint32_t filter_table[FILTER_TABLE_SIZE];
    float meas_voltage;
} meas_data_t;

static meas_data_t meas_data[MEAS_CH_LAST] =
{
    [MEAS_CH_IN] =          {.unit = 1, .channel = ADC_IN_CH,     .ch_name         = "MEAS_CH_IN"         },
    [MEAS_CH_MOTOR] =       {.unit = 1, .channel = ADC_MOTOR_CH,  .ch_name         = "MEAS_CH_MOTOR"      },
    [MEAS_CH_12V] =         {.unit = 1, .channel = ADC_12V_CH,    .ch_name         = "MEAS_CH_12V"        },
#if CONFIG_DEVICE_SIEWNIK
    [MEAS_CH_SERVO] =       {.unit = 1, .channel = ADC_SERVO_CH,  .ch_name         = "MEAS_CH_SERVO"      },
    [MEAS_CH_TEMP] =        {.unit = 1, .channel = ADC_CE_CH,     .ch_name         = "MEAS_CH_TEMP"       },
#endif

#if CONFIG_DEVICE_SOLARKA
    [MEAS_CH_TEMP] =        {.unit = 1, .channel = ADC_CHANNEL_4, .ch_name         = "MEAS_CH_TEMP"       },
    [MEAS_CH_CHECK_VIBRO] = {.unit = 1, .channel = ADC_CHANNEL_0, .ch_name         = "MEAS_CH_CHECK_VIBRO"},
    [MEAS_CH_CHECK_MOTOR] = {.unit = 1, .channel = ADC_CHANNEL_3, .ch_name         = "MEAS_CH_CHECK_MOTOR"},
#endif
};

static uint32_t table_size;
static uint32_t table_iter;
uint32_t motor_calibration_meas;
#if CONFIG_DEVICE_SOLARKA
static TimerHandle_t motorCalibrationTimer;
#endif

#if CONFIG_DEVICE_SIEWNIK
static TimerHandle_t servoCalibrationTimer;
static uint32_t calibration_value;
static void measure_get_servo_calibration(TimerHandle_t xTimer)
{
    calibration_value = measure_get_filtered_value(MEAS_CH_SERVO);
    LOG(PRINT_INFO, "MEASURE SERVO Calibration value = %d", calibration_value);
}

#endif

static void measure_get_motor_calibration(TimerHandle_t xTimer)
{
    if (!menuGetValue(MENU_MOTOR_IS_ON))
    {
        motor_calibration_meas = measure_get_filtered_value(MEAS_CH_MOTOR);
        LOG(PRINT_INFO, "MEASURE MOTOR Calibration value = %d", motor_calibration_meas);
    }
    else
    {
        LOG(PRINT_INFO, "MEASURE MOTOR Fail get. Motor is on");
    }
}

static uint32_t filtered_value(uint32_t *tab, uint8_t size)
{
    uint16_t ret_val = *tab;

    for (uint8_t i = 1; i < size; i++)
    {
        ret_val = (ret_val + tab[i]) / 2;
    }

    return ret_val;
}

void init_measure(void)
{
    motor_calibration_meas = DEFAULT_MOTOR_CALIBRATION_VALUE;
}

static void _read_adc_values(void)
{
    for (uint8_t ch = 0; ch < MEAS_CH_LAST; ch++)
    {
        meas_data[ch].adc = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++)
        {
            if (meas_data[ch].unit == ADC_UNIT_1)
            {
                meas_data[ch].adc += adc1_get_raw((adc1_channel_t)meas_data[ch].channel);
            }
            else
            {
                int raw = 0;
                adc2_get_raw((adc2_channel_t)meas_data[ch].channel, width, &raw);
                meas_data[ch].adc += raw;
            }
        }

        meas_data[ch].adc /= NO_OF_SAMPLES;
        meas_data[ch].filter_table[table_iter % FILTER_TABLE_SIZE] = meas_data[ch].adc;
        meas_data[ch].filtered_adc = filtered_value(&meas_data[ch].adc, table_size);
    }

    table_iter++;
    if (table_size < FILTER_TABLE_SIZE - 1)
    {
        table_size++;
    }
}

#define SILOS_START_MEASURE    100
#define SILOS_LOW              600

static void measure_process(void *arg)
{
    (void)arg;
    while (1)
    {
        vTaskDelay(100 / portTICK_RATE_MS);

        _read_adc_values();

        // LOG(PRINT_INFO, "%s %d", meas_data[MEAS_CH_CHECK_VIBRO].ch_name, meas_data[MEAS_CH_CHECK_VIBRO].filtered_adc);
        // LOG(PRINT_INFO, "%s %d", meas_data[MEAS_CH_CHECK_MOTOR].ch_name, meas_data[MEAS_CH_CHECK_MOTOR].filtered_adc);

        uint32_t silos_distance = ultrasonar_get_distance() >
            SILOS_START_MEASURE ? ultrasonar_get_distance() - SILOS_START_MEASURE : 0;
        if (silos_distance > SILOS_LOW)
        {
            silos_distance = SILOS_LOW;
        }

        int silos_percent = (SILOS_LOW - silos_distance) * 100 / SILOS_LOW;
        if ((silos_percent < 0) || (silos_percent > 100))
        {
            silos_percent = 0;
        }

        menuSetValue(MENU_LOW_LEVEL_SILOS, silos_percent < 10);
        menuSetValue(MENU_SILOS_LEVEL, (uint32_t)silos_percent);
        menuSetValue(MENU_VOLTAGE_ACCUM, (uint32_t)(accum_get_voltage() * 10000.0));
        menuSetValue(MENU_CURRENT_MOTOR, (uint32_t)(measure_get_current(MEAS_CH_MOTOR, 0.1)));
        menuSetValue(MENU_TEMPERATURE, (uint32_t)(measure_get_temperature()));
        /* DEBUG */
        // menuPrintParameter(MENU_VOLTAGE_ACCUM);
        // menuPrintParameter(MENU_CURRENT_MOTOR);
        // menuPrintParameter(MENU_TEMPERATURE);
    }
}

void measure_start(void)
{
    adc1_config_width(width);
    adc1_config_channel_atten(ADC_CHANNEL_4, atten);
    adc1_config_channel_atten(ADC_CHANNEL_5, atten);
    adc1_config_channel_atten(ADC_CHANNEL_6, atten);
    adc1_config_channel_atten(ADC_CHANNEL_7, atten);
    adc2_config_channel_atten((adc2_channel_t)ADC_CHANNEL_5, atten);

    xTaskCreate(measure_process, "measure_process", 4096, NULL, 10, NULL);
#if CONFIG_DEVICE_SIEWNIK
    servoCalibrationTimer = xTimerCreate("servoCalibrationTimer", 1000 / portTICK_RATE_MS, pdFALSE, (void *)0,
            measure_get_servo_calibration);
#endif

    motorCalibrationTimer = xTimerCreate("motorCalibrationTimer", 1000 / portTICK_RATE_MS, pdFALSE, (void *)0,
            measure_get_motor_calibration);

    init_measure();
}

void measure_meas_calibration_value(void)
{
    LOG(PRINT_INFO, "%s", __func__);
#if CONFIG_DEVICE_SIEWNIK
    xTimerStart(servoCalibrationTimer, 0);
#endif
    xTimerStart(motorCalibrationTimer, 0);
}

uint32_t measure_get_filtered_value(enum_meas_ch type)
{
    if (type < MEAS_CH_LAST)
    {
        return meas_data[type].filtered_adc;
    }

    return 0;
}

float measure_get_temperature(void)
{
#if CONFIG_DEVICE_SIEWNIK
    int temp = -((int)measure_get_filtered_value(MEAS_CH_TEMP)) / 82 + 41;
    LOG(PRINT_DEBUG, "Temperature %d %d", measure_get_filtered_value(MEAS_CH_TEMP), temp);
    return temp;
#endif

#if CONFIG_DEVICE_SOLARKA
    int temp = -((int)measure_get_filtered_value(MEAS_CH_TEMP)) / 32 + 130;
    LOG(PRINT_DEBUG, "Temperature %d %d", measure_get_filtered_value(MEAS_CH_TEMP), temp);
    return temp;
#endif
}

float measure_get_current(enum_meas_ch type, float resistor)
{
#if CONFIG_DEVICE_SIEWNIK
    LOG(PRINT_DEBUG, "Adc %d calib %d", measure_get_filtered_value(type), motor_calibration_meas);
    uint32_t adc = measure_get_filtered_value(type) <
        motor_calibration_meas ? 0 : measure_get_filtered_value(type) - motor_calibration_meas;
    float current = (float)adc / 1.1 /* Amp */;
    LOG(PRINT_DEBUG, "Adc %d calib %d curr %f", adc, motor_calibration_meas, current);
#endif

#if CONFIG_DEVICE_SOLARKA
    LOG(PRINT_DEBUG, "Adc %d calib %d", measure_get_filtered_value(type), motor_calibration_meas);
    uint32_t adc = measure_get_filtered_value(type) <
        motor_calibration_meas ? 0 : measure_get_filtered_value(type) - motor_calibration_meas;

    float voltage = menuGetValue(MENU_VOLTAGE_ACCUM) / 100.0;
    float correction = (14.2 - voltage) * 100;
    float current_meas = (float)adc * 0.92;
    float current = current_meas/* + correction*//* Amp */;
    LOG(PRINT_DEBUG, "voltage %f correction %f current_meas %f current %f", voltage, correction, current_meas, current);
    LOG(PRINT_DEBUG, "Adc %d calib %d curr %f", adc, motor_calibration_meas, current);
#endif

    return current;
}

float accum_get_voltage(void)
{
    float voltage = 0;

#if CONFIG_DEVICE_SOLARKA
    voltage = (float)measure_get_filtered_value(MEAS_CH_12V) / 4096.0 / 2.5;
#else
    voltage = (float)measure_get_filtered_value(MEAS_CH_12V) / 4096.0 / 2.6;
#endif
    return voltage;
}
