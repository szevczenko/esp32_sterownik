#include "config.h"
#include "vibro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

vibro_t vibroD;

#if MENU_VIRO_ON_OFF_VERSION
void vibro_config(uint32_t vibro_on_ms, uint32_t vibro_off_ms)
#else
void vibro_config(uint32_t period, uint32_t filling)
#endif
{
    #if MENU_VIRO_ON_OFF_VERSION
    vibroD.vibro_on_ms = vibro_on_ms;
    vibroD.vibro_off_ms = vibro_off_ms;
    #else
    vibroD.period = period < 10 ? 10 : period;
    vibroD.filling = filling > 100 ? 100 : filling;
    #endif
    if (vibroD.state < VIBRO_STATE_CONFIGURED)
    {
        vibroD.state = VIBRO_STATE_CONFIGURED;
    }
}

void vibro_start(void)
{
    if (vibroD.state == VIBRO_STATE_NO_INIT)
    {
        return;
    }

    vibroD.state = VIBRO_STATE_START;
}

void vibro_stop(void)
{
    if (vibroD.state == VIBRO_STATE_NO_INIT)
    {
        return;
    }

    vibroD.state = VIBRO_STATE_STOP;
    vibroD.type = VIBRO_TYPE_OFF;
}

uint8_t vibro_is_on(void)
{
    return vibroD.type == VIBRO_TYPE_ON;
}

uint8_t vibro_is_started(void)
{
    return vibroD.state == VIBRO_STATE_START;
}

static void vibro_process(void *pv)
{
    while (1)
    {
        if (vibroD.filling == 0)
        {
            vibroD.type = VIBRO_TYPE_OFF;
            vTaskDelay(MS2ST(100));
            continue;
        }

        if (vibroD.state == VIBRO_STATE_START)
        {
            vibroD.type = VIBRO_TYPE_ON;
            vibroD.vibro_on_start_time = xTaskGetTickCount();

            while (vibroD.state == VIBRO_STATE_START)
            {
#if MENU_VIRO_ON_OFF_VERSION
                if (vibroD.vibro_on_start_time + MS2ST(vibroD.vibro_on_ms) < xTaskGetTickCount())
                {
                    break;
                }
#else
                uint32_t vibro_on_ms = vibroD.period * vibroD.filling / 100;
                if (vibroD.vibro_on_start_time + MS2ST(vibro_on_ms) < xTaskGetTickCount())
                {
                    break;
                }
#endif
                osDelay(100);
            }
#if MENU_VIRO_ON_OFF_VERSION
            if (vibroD.vibro_off_ms != 0)
#else
            if (vibroD.filling != 0)
#endif
            {
                vibroD.type = VIBRO_TYPE_OFF;
                vibroD.vibro_off_start_time = xTaskGetTickCount();
                while (vibroD.state == VIBRO_STATE_START)
                {
#if MENU_VIRO_ON_OFF_VERSION
                    if (vibroD.vibro_off_start_time + MS2ST(vibroD.vibro_off_ms) < xTaskGetTickCount())
                    {
                        break;
                    }
#else
                    uint32_t vibro_off_ms = (vibroD.period - vibroD.period * vibroD.filling / 100);
                    if (vibroD.vibro_off_start_time + MS2ST(vibro_off_ms) < xTaskGetTickCount())
                    {
                        break;
                    }
#endif
                    osDelay(100);
                }
            }
        }
        else
        {
            vibroD.type = VIBRO_TYPE_OFF;
            vTaskDelay(MS2ST(100));
        }
    }
}

void vibro_init(void)
{
    memset(&vibroD, 0, sizeof(vibroD));
    vibroD.state = VIBRO_STATE_READY;
    xTaskCreate(vibro_process, "vibro_process", 4096, NULL, 10, NULL);
}
