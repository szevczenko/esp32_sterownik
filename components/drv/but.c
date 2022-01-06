/*
 * but.c
 *
 * Created: 05.02.2019 17:20:37
 *  Author: Demetriusz
 */ 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "config.h"
#include "stdint.h"
#include "but.h"
#include "pcf8574.h"
#include "buzzer.h"




extern uint8_t test_number;

but_t button1, button2, button3, button4, button5, button6, button7, button8, button9, button10;
but_t *but_tab[BUTTON_CNT] = {&button1, &button2, &button3, &button4, &button5, &button6, &button7, &button8, &button9, &button10};
static int read_i2c_value;

uint8_t read_button(but_t *but)
{
	if (but->is_gpio){
		return gpio_get_level(but->gpio);
	}
	return !(~read_i2c_value & (1 << but->bit));
}

extern uint8_t test_button;
void test_fnc(void * pv)
{
}

void init_but_struct(void)
{
	button1.state = 0;
	button1.value = 1;
	button1.fall_callback = 0;
	button1.rise_callback = 0;
	button1.timer_callback = 0;//test_fnc;
	button1.gpio = BUT1_GPIO;
	button1.bit = 4;
	
	button2.state = 0;
	button2.value = 1;
	button2.fall_callback = 0;
	button2.rise_callback = 0;
	button2.timer_callback = 0;
	button2.gpio = BUT2_GPIO;
	button2.bit = 5;
	
	button3.state = 0;
	button3.value = 1;
	button3.fall_callback = 0;
	button3.rise_callback = 0;
	button3.timer_callback = 0;
	button3.gpio = BUT3_GPIO;
	button3.bit = 6;
	
	button4.state = 0;
	button4.value = 1;
	button4.fall_callback = 0;
	button4.rise_callback = 0;
	button4.timer_callback = 0;
	button4.gpio = BUT4_GPIO;
	button4.bit = 1;
	
	button5.state = 0;
	button5.value = 1;
	button5.fall_callback = 0;
	button5.rise_callback = 0;
	button5.timer_callback = 0;
	button5.gpio = BUT5_GPIO;
	button5.bit = 7;
	
	button6.state = 0;
	button6.value = 1;
	button6.fall_callback = 0;
	button6.rise_callback = 0;
	button6.timer_callback = 0;
	button6.gpio = BUT6_GPIO;
	button6.bit = 0;
	
	button7.state = 0;
	button7.value = 1;
	button7.fall_callback = 0;
	button7.rise_callback = 0;
	button7.timer_callback = 0;
	button7.gpio = BUT7_GPIO;
	button7.bit = 3;
	
	button8.state = 0;
	button8.value = 1;
	button8.fall_callback = 0;
	button8.rise_callback = 0;
	button8.timer_callback = 0;
	button8.gpio = BUT8_GPIO;
	button8.bit = 2;
	
	button9.state = 0;
	button9.value = 1;
	button9.fall_callback = 0;
	button9.rise_callback = 0;
	button9.timer_callback = 0;
	button9.gpio = BUT9_GPIO;
	button9.is_gpio = 1;
	
	button10.state = 0;
	button10.value = 1;
	button10.fall_callback = 0;
	button10.rise_callback = 0;
	button10.timer_callback = 0;
	button10.gpio = BUT10_GPIO;
	button10.bit = 4;
	button10.is_gpio = 1;
}

static uint32_t start_time;
static void process_button(void * arg)
{
	uint8_t red_val = 0;
	
	while(1)
	{
		WDT_FEED();
		start_time = xTaskGetTickCount();
		//process
		read_i2c_value = pcf8574_getinput(0);
		//taskENTER_CRITICAL();
		for (uint8_t i=0; i<BUTTON_CNT; i++)
		{
			red_val = read_button(but_tab[i]);
			if(red_val != but_tab[i]->value)
			{
				but_tab[i]->value = red_val;
				if (red_val == 1) {
					if (but_tab[i]->rise_callback != 0) {
						but_tab[i]->rise_callback(but_tab[i]->arg);
					}
				}
				else if(red_val == 0 && but_tab[i]->fall_callback != 0) {
					buzzer_click();
					but_tab[i]->fall_callback(but_tab[i]->arg);
				}
				
			}
			//timer
			if (red_val == 0)
			{
				but_tab[i]->tim_cnt++;
				if ((but_tab[i]->tim_cnt >= TIMER_CNT_TIMEOUT) && (but_tab[i]->state != 1))
				{
					if (but_tab[i]->timer_callback != 0)
					but_tab[i]->timer_callback(but_tab[i]->arg);
					but_tab[i]->tim_cnt = 0;
					but_tab[i]->state = 1;
				}
			}
			else
			{
				but_tab[i]->tim_cnt = 0;
				but_tab[i]->state = 0;
			}
		} // end for
		//taskEXIT_CRITICAL();
		vTaskDelay(30 / portTICK_RATE_MS);
	}// end while
}

static void set_bit_mask(uint32_t *mask)
{
	for(uint8_t i = 0; i < BUTTON_CNT; i++)
	{
		if (but_tab[i]->is_gpio)
			*mask |= (1ULL<<but_tab[i]->gpio);
	}
}

void init_buttons(void)
{
	pcf8574_init();
	gpio_config_t io_conf;
	//disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
	init_but_struct();
	//interrupt of rising edge
	io_conf.pin_bit_mask = 0;
    //bit mask of the pins, use GPIO4/5 here
	set_bit_mask(&io_conf.pin_bit_mask);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
	xTaskCreate(process_button, "gpio_task", 4096, NULL, 13, NULL);
}