#include "config.h"
#include "freertos/timers.h"
#include "menu.h"
#include "but.h"
#include "ssd1306.h"
#include "ssdFigure.h"

#include "freertos/semphr.h"
#include "wifidrv.h"
#include "cmd_client.h"
#include "battery.h"
#include "vibro.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "buzzer.h"
#include "keepalive.h"

#define WAKE_UP_PIN 12

typedef enum
{
	SLEEP_STATE_NO_INIT,
	SLEEP_STATE_NO_SLEEP,
	SLEEP_STATE_GO_TO_SLEEP,
	SLEEP_STATE_START_SLEEPING,
	SLEEP_STATE_UNSLEEP_PROCESS,
	SLEEP_STATE_WAKE_UP,
}sleep_state_t;

static sleep_state_t sleep_state;
static uint8_t sleep_signal;

bool is_sleeping(void)
{
	return (sleep_state != SLEEP_STATE_NO_SLEEP) || (sleep_signal == 1);
}

void go_to_sleep(void)
{
	sleep_signal = 1;
}

void go_to_wake_up(void)
{
	sleep_signal = 0;
}

static void sleep_task(void * arg)
{
	while(1)
	{
		
		switch(sleep_state)
		{
			case SLEEP_STATE_NO_SLEEP:
				if (sleep_signal == 1)
				{
					sleep_state = SLEEP_STATE_GO_TO_SLEEP;
				}
				break;

			case SLEEP_STATE_GO_TO_SLEEP:
				vTaskDelay(MS2ST(3000));
				if (sleep_signal == 1)
				{
					sleep_state = SLEEP_STATE_START_SLEEPING;
				}
				else
				{
					sleep_state = SLEEP_STATE_WAKE_UP;
				}
				break;

			case SLEEP_STATE_START_SLEEPING:
				debug_msg("SLEEP_STATE_START_SLEEPING\n\r");
				esp_sleep_enable_timer_wakeup(10 * 1000000);
				esp_sleep_enable_gpio_wakeup();
				gpio_wakeup_enable(WAKE_UP_PIN, GPIO_INTR_LOW_LEVEL);
				esp_wifi_stop();
				ssd1306_Fill(Black);
				ssd1306_UpdateScreen();
				BUZZER_OFF();
				esp_light_sleep_start();
				vTaskDelay(MS2ST(3000));
				if (sleep_signal == 0)
				{
					sleep_state = SLEEP_STATE_WAKE_UP;
				}
				else
				{
					sleep_state = SLEEP_STATE_UNSLEEP_PROCESS;
				}
				break;

			case SLEEP_STATE_UNSLEEP_PROCESS:
				debug_msg("SLEEP_STATE_UNSLEEP_PROCESS\n\r");
				if (wifiDrvConnect() == ESP_OK)
				{
					debug_msg("wifiDrvConnected\n\r");
					if(!cmdClientIsConnected())
					{
						if (cmdClientTryConnect(3000) == 1)
						{
							debug_msg("cmdClientTryConnected\n\r");
							sendKeepAliveFrame();
							vTaskDelay(MS2ST(1000));
						}
					}
					debug_msg("cmdClientConnect or wifiDrvConnected error\n\r");
				}

				BUZZER_ON();
				vTaskDelay(MS2ST(100));
				BUZZER_OFF();

				if (sleep_signal == 0)
				{
					sleep_state = SLEEP_STATE_WAKE_UP;
					debug_msg("SLEEP_STATE_WAKE_UP\n\r");
				}
				else
				{
					sleep_state = SLEEP_STATE_START_SLEEPING;
				}

				break;


			case SLEEP_STATE_WAKE_UP:
				if (sleep_signal == 1)
				{
					sleep_state = SLEEP_STATE_GO_TO_SLEEP;
				}
				else
				{
					sleep_state = SLEEP_STATE_NO_SLEEP;
				}
				break;

			default:
				break;
		}
		vTaskDelay(MS2ST(250));
	}
}

void init_sleep(void)
{
	xTaskCreate(sleep_task, "sleep_task", 1024, NULL, 5, NULL);
	sleep_state = SLEEP_STATE_NO_SLEEP;
}