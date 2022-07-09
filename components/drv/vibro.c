#include "config.h"
#include "vibro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

vibro_t vibroD;

void vibro_config(uint32_t period, uint32_t working_time)
{
    if (period < working_time)
    {
        working_time = period;
    }

    vibroD.period = period;
    vibroD.working_time = working_time;
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
        if (vibroD.state == VIBRO_STATE_START)
        {
            if (vibroD.period <= vibroD.working_time)
            {
                vibroD.type = VIBRO_TYPE_ON;
                vTaskDelay(MS2ST(100));
            }
            else if (vibroD.type == VIBRO_TYPE_OFF)
            {
                vibroD.type = VIBRO_TYPE_ON;
                vTaskDelay(MS2ST(vibroD.working_time));
            }
            else if (vibroD.type == VIBRO_TYPE_ON)
            {
                uint32_t diff = vibroD.period - vibroD.working_time;
                vibroD.type = VIBRO_TYPE_OFF;
                vTaskDelay(MS2ST(diff));
            }
        }
        else
        {
            vibroD.type = VIBRO_TYPE_OFF;
            vTaskDelay(MS2ST(250));
        }

        //LOG("period %d working_time %d, type %d state %d", vibroD.period, vibroD.working_time, vibroD.type, vibroD.state);
    }
}

void vibro_init(void)
{
    memset(&vibroD, 0, sizeof(vibroD));
    vibroD.state = VIBRO_STATE_READY;
    xTaskCreate(vibro_process, "vibro_process", 4096, NULL, 10, NULL);
}
