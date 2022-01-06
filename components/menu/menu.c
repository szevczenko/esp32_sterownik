#include "config.h"
#include "freertos/timers.h"
#include "menu.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "but.h"

#include "semphr.h"
#include "wifidrv.h"
#include "cmd_client.h"
#include "fast_add.h"
#include "battery.h"
#include "vibro.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "buzzer.h"
#include "sleep_e.h"

//#undef debug_msg
//#define debug_msg(...) //debug_msg( __VA_ARGS__)
#define CONFIG_MENU_TEST_TASK 0
#define LINE_HEIGHT 10
#define MENU_HEIGHT 18
#define MAX_LINE (SSD1306_HEIGHT - MENU_HEIGHT) / LINE_HEIGHT

#if 0

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif



static char wifi_device_list[16][33];
static char devName[33];
static char infoBuff[256];
static uint16_t dev_count;
static TickType_t connect_timeout;
static TickType_t animation_timeout;

static uint8_t animation_cnt;
static uint8_t parameters_menu_flag;
static TimerHandle_t xTimers;

static stateWifiMenu_t wifiState;

uint32_t motor_value;
uint32_t servo_value;
bool motor_on;
static int line_start, line_end, last_button;
static int menu_start_line;

#if CONFIG_DEVICE_SOLARKA
uint32_t menu_start_period_value, menu_start_wt_value;
#endif

bool servo_vibro_on;

static scrollBar_t scrollBar = {
	.line_max = MAX_LINE,
	.y_start = MENU_HEIGHT
};

loadBar_t motor_bar = {
    .x = 40,
    .y = 10,
    .width = 80,
    .height = 10,
};

loadBar_t servo_bar = {
    .x = 40,
    .y = 40,
    .width = 80,
    .height = 10,
};

extern menu_token_t *setting_tokens[];

menu_token_t wifi_menu = 
{
	.name = "DEVICES",
	.arg_type = T_ARG_TYPE_WIFI,
};

menu_token_t info_menu = 
{
	.name = "INFO",
	.arg_type = T_ARG_TYPE_INFO,
};

menu_token_t parameters_menu = 
{
	.name = "PARAMETERS",
	.arg_type = T_ARG_TYPE_PARAMETERS,
};

menu_token_t setings = 
{
	.name = "SETINGS",
	.arg_type = T_ARG_TYPE_MENU,
	.menu_list = setting_tokens
};

menu_token_t* main_menu_tokens[] = {&start_menu, &setings, &wifi_menu, NULL} ;

menu_token_t main_menu = 
{
	.name = "MENU",
	.arg_type = T_ARG_TYPE_MENU,
	.menu_list = main_menu_tokens
};

menu_token_t sleep_menu = 
{
	.name = "SLEEP",
	.arg_type = T_ARG_TYPE_SLEEP,
};


void menu_test(void * arg) ;
static void menu_task(void * arg);
void menuEnterStartProcess(void);
void menuEnterParametersMenu(void);

static void error_reset(void) {

	if (cmdClientSendCmdI(PC_CMD_RESET_ERROR) == FALSE) {
		return;
	}
	menuSetValue(MENU_MOTOR_ERROR_IS_ON, 0);
	menuSetValue(MENU_SERVO_ERROR_IS_ON, 0);
	motor_on = 0;
	servo_vibro_on = 0;
}

void menu_start_find_device_I(void)
{
	if (last_tab_element() != &wifi_menu) return;
	wifiState = ST_WIFI_FIND_DEVICE;
	dev_count = 0;
	uint16_t ap_count = 0;
	char dev_name[33];
	wifiDrvStartScan();
	wifiDrvGetScanResult(&ap_count);
	for (uint16_t i = 0; i < ap_count; i++)
	{
		wifiDrvGetNameFromScannedList(i, dev_name);
		if (memcmp(dev_name, WIFI_AP_NAME, strlen(WIFI_AP_NAME) - 1) == 0)
		{
			debug_msg("%s\n", dev_name);
			strcpy(wifi_device_list[dev_count++], dev_name);
		}
	}
	debug_msg("Dev found %d\n", dev_count);
	wifiState = ST_WIFI_DEVICE_LIST;
}

void menu_start_find_device(void)
{
	taskEXIT_CRITICAL();
	menu_start_find_device_I();
	taskENTER_CRITICAL();
}

static int connectToDevice(char *dev)
{

	if (memcmp(WIFI_AP_NAME, dev, strlen(WIFI_AP_NAME) - 1) == 0)
	{
		if (wifiDrvIsConnected()){
			if (wifiDrvDisconnect() != ESP_OK){
				//debug_msg("MENU: Disconnect wifi failed");
				menuPrintfInfo("Disconnect wifi failed");
			}
		}
		strcpy(devName, dev);
		//debug_msg("MENU: Try connect %s", devName);
		entered_menu_tab[1] = &wifi_menu;
		for (uint8_t i = 2; i < 8; i++)
		{
			entered_menu_tab[i] = NULL;
		}
		wifiDrvSetAPName(dev, strlen(dev));
		wifiDrvSetPassword(WIFI_AP_PASSWORD, strlen(WIFI_AP_PASSWORD));
		if (wifiDrvConnect() != ESP_OK) {
			//debug_msg("MENU: wifiDrvConnect failed");
			menuPrintfInfo("wifiDrvConnect failed");
			return FALSE;
		}
		wifiState = ST_WIFI_DEVICE_TRY_CONNECT;
		connect_timeout = xTaskGetTickCount() + MS2ST(3000);
		update_screen();
		return TRUE;
	}
	return FALSE;
}

void button_up_callback(void * arg)
{
	last_button = 1;
	menu_token_t * menu = last_tab_element();
	
	if (menu == NULL) 
	{
		debug_msg("Error button_up_callback: menu == NULL\n");
		return;
	}
	debug_function_name("button_up_callback");
	switch(menu->arg_type)
	{
		case T_ARG_TYPE_START:
			menu_start_line = 0;
			update_screen();
			break;

		case T_ARG_TYPE_WIFI:
				if (wifiState == ST_WIFI_DEVICE_CONNECTED_FAILED || (wifiState == ST_WIFI_DEVICE_LIST && dev_count == 0)) {
					menu_start_find_device();
				}
				else if(menu->position > 0) {
					menu->position--;
					update_screen();
				}
			break;

		default:
			if (menu->position > 0) {
				menu->position--;
				update_screen();
			}
			break;
	}
	//debug_msg("button_up_callback Len: %d, Pos: %d\n", len_menu(menu), menu->position);
}

void button_down_callback(void * arg)
{
	last_button = 0;
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) 
	{
		debug_msg("Error button_down_callback: menu == NULL\n");
		return;
	}
	debug_function_name("button_down_callback");
	switch(menu->arg_type)
	{
		case T_ARG_TYPE_START:
			menu_start_line = 1;
			update_screen();
			break;

		case T_ARG_TYPE_WIFI:
				if (wifiState == ST_WIFI_DEVICE_CONNECTED_FAILED || (wifiState == ST_WIFI_DEVICE_LIST && dev_count == 0)) {
					menu_start_find_device();
				}
				else if (menu->position < dev_count - 1) {
					menu->position++;
					update_screen();
				}	
			break;

		default:
			if (menu->position < len_menu(menu) - 1) {
				menu->position++;
				update_screen();
			}
			break;
	}
	//debug_msg("button_down_callback Len: %d, Pos: %d\n", len_menu(menu), menu->position);
}

void button_plus_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	debug_function_name("button_plus_callback");
	parameters_menu_flag++;
	if (parameters_menu_flag == 2) {
		menuEnterParametersMenu();
		return;
	}
	switch(menu->arg_type)
	{
		case T_ARG_TYPE_BOOL:
			if (menu->value == NULL) return;
			if ((bool)*menu->value == (bool)0) {
				*menu->value = (bool)1;
				save_parameters();
			}
		break;

		case T_ARG_TYPE_VALUE:
			if (menu->value == NULL) return;
			if (*menu->value < 100) {
				*menu->value = *menu->value + 1;
				save_parameters();
			}
		break;

		case T_ARG_TYPE_MENU:
			if (menu->menu_list == NULL || menu->menu_list[menu->position] == NULL) return;
			add_menu_tab(menu->menu_list[menu->position]);
			if (last_tab_element() == &wifi_menu)
			{
				//debug_msg("Enter wifi_menu\n");
				menu_start_find_device();
			}
		break;

		case T_ARG_TYPE_START:
			if (motor_value < 100) {
				motor_value++;
				cmdClientSetValueWithoutRespI(MENU_MOTOR, motor_value);
			}
				
		break;

		case T_ARG_TYPE_WIFI:
			if (wifiState == ST_WIFI_DEVICE_CONNECTED_FAILED || (wifiState == ST_WIFI_DEVICE_LIST && dev_count == 0)) {
				menu_start_find_device();
			}
			else if (wifiState == ST_WIFI_DEVICE_LIST) {
				connectToDevice(wifi_device_list[menu->position]);
			}
			break;

		default:
			return;
	}
	update_screen();
}

void motor_fast_add(uint32_t value) {
	(void) value;
	cmdClientSetValueWithoutResp(MENU_MOTOR, motor_value);
	//debug_msg("Motor value %d\n", motor_value);
}

void button_plus_time_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	switch(menu->arg_type){

		case T_ARG_TYPE_START:
			fastProcessStart(&motor_value, 100, 0, FP_PLUS, motor_fast_add);
		break;

		default:
			return;
	}
	update_screen();
}

void button_plus_up_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	parameters_menu_flag--;
	switch(menu->arg_type)
	{
		
		case T_ARG_TYPE_START:
			fastProcessStop(&motor_value);
			save_parameters();
		break;

		default:
			return;
	}
	update_screen();
}

void button_minus_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	parameters_menu_flag++;
	if (parameters_menu_flag == 2) {
		menuEnterParametersMenu();
		return;
	}
	debug_function_name("button_minus_callback");
	switch(menu->arg_type)
	{
		case T_ARG_TYPE_BOOL:
			if (menu->value == NULL) return;
			if ((bool)*menu->value == 1) {
				*menu->value = (bool)0;
				save_parameters();
			}
		break;

		case T_ARG_TYPE_VALUE:
			if (menu->value == NULL) return;
			if (*menu->value > 0) {
				*menu->value = *menu->value - 1;
				save_parameters();
			}
		break;

		case T_ARG_TYPE_MENU:
			remove_last_menu_tab();
		break;

		case T_ARG_TYPE_WIFI:
			if (wifiState == ST_WIFI_DEVICE_CONNECTED_FAILED || (wifiState == ST_WIFI_DEVICE_LIST && dev_count == 0)) {
				//menu_start_find_device();
				remove_last_menu_tab();
			}
			else if (wifiState == ST_WIFI_DEVICE_LIST) {
				remove_last_menu_tab();
			}
			break;

		case T_ARG_TYPE_START:
			if (motor_value > 0) {
				motor_value--;
				cmdClientSetValueWithoutRespI(MENU_MOTOR, motor_value);
			}
		break;

		default:
			return;
	}
	update_screen();
}

void button_minus_time_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	switch(menu->arg_type){

		case T_ARG_TYPE_START:
			fastProcessStart(&motor_value, 100, 0, FP_MINUS, motor_fast_add);
		break;

		default:
			return;
	}
	update_screen();
}

void button_minus_up_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	parameters_menu_flag--;
	switch(menu->arg_type)
	{
		
		case T_ARG_TYPE_START:
			fastProcessStop(&motor_value);
			save_parameters();
		break;

		default:
			return;
	}
	update_screen();
}

void button_enter_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	//debug_msg("Enter cur (%s)\n", menu->name);
	debug_function_name("button_enter_callback");
	switch(menu->arg_type)
	{
		case T_ARG_TYPE_BOOL:
			remove_last_menu_tab();
		break;

		case T_ARG_TYPE_VALUE:
			remove_last_menu_tab();
		break;

		case T_ARG_TYPE_MENU:
			if (menu->menu_list == NULL || menu->menu_list[menu->position] == NULL) return;
			add_menu_tab(menu->menu_list[menu->position]);
			if (last_tab_element() == &wifi_menu)
			{
				menu_start_find_device();
			}
			else if (last_tab_element() == &start_menu) {
				menuEnterStartProcess();
			}
		break;

		case T_ARG_TYPE_START:
			if (menuGetValue(MENU_MOTOR_ERROR_IS_ON) || menuGetValue(MENU_SERVO_ERROR_IS_ON)) {
				error_reset();
			}
			else {
				remove_last_menu_tab();
			}
		break;

		case T_ARG_TYPE_WIFI:
			if (wifiState == ST_WIFI_DEVICE_LIST) {
				connectToDevice(wifi_device_list[menu->position]);
			}
			else if (wifiState == ST_WIFI_DEVICE_CONNECTED_FAILED || (wifiState == ST_WIFI_DEVICE_LIST && dev_count == 0)) {
				menu_start_find_device();
			}
		break;

		default:
			return;
	}
	update_screen();
}

void button_exit_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	//debug_msg("Exit\n", len_menu(menu), menu->position);
	if (menu == NULL) return;
	debug_function_name("button_exit_callback");
	switch(menu->arg_type)
	{
		case T_ARG_TYPE_BOOL:
			remove_last_menu_tab();
		break;

		case T_ARG_TYPE_VALUE:
			remove_last_menu_tab();
		break;

		case T_ARG_TYPE_MENU:
			remove_last_menu_tab();
		break;

		case T_ARG_TYPE_START:
			xTimerStop(xTimers, 0);
			if (servo_vibro_on) {
				servo_vibro_on = 0;
				cmdClientSetValueWithoutRespI(MENU_SERVO_IS_ON, servo_vibro_on);
			}
			else {
				servo_vibro_on = 1;
				cmdClientSetValueWithoutRespI(MENU_SERVO_IS_ON, servo_vibro_on);
			}
		break;

		case T_ARG_TYPE_WIFI:
			remove_last_menu_tab();
		break;

		default:
			return;
	}
	update_screen();
}

static void timerCallback(void * pv) {
	servo_vibro_on = 1;
	cmdClientSetValueWithoutResp(MENU_SERVO_IS_ON, servo_vibro_on);
}

void button_plus_servo_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	debug_function_name("button_plus_servo_callback");
	switch(menu->arg_type)
	{
	
		case T_ARG_TYPE_START:
		#if CONFIG_DEVICE_SOLARKA
			if (menu_start_line) {
				if (menu_start_wt_value < 100) {
					menu_start_wt_value++;
					/* vibro value change */
					cmdClientSetValueWithoutRespI(MENU_VIBRO_WORKING_TIME, menu_start_wt_value);
				}
			}
			else {
				if (menu_start_period_value < 100) {
					menu_start_period_value++;
					/* vibro value change */
					cmdClientSetValueWithoutRespI(MENU_VIBRO_PERIOD, menu_start_period_value);
				}
			}
		#elif CONFIG_DEVICE_SIEWNIK
			if (servo_value < 100) {
				servo_value++;
				cmdClientSetValueWithoutRespI(MENU_SERVO, servo_value);
			}
		#endif
		break;

		default:
			return;
	}
	update_screen();
}

static void servo_fast_add(uint32_t value) {
	(void) value;
	#if CONFIG_DEVICE_SOLARKA
	if (menu_start_line) {
		cmdClientSetValueWithoutResp(MENU_VIBRO_WORKING_TIME, menu_start_wt_value);
	}
	else {
		cmdClientSetValueWithoutResp(MENU_VIBRO_PERIOD, menu_start_period_value);
	}
	#elif CONFIG_DEVICE_SIEWNIK
	cmdClientSetValueWithoutResp(MENU_SERVO, servo_value);
	#endif
	//debug_msg("servo_value %d\n", servo_value);
}

void button_plus_servo_time_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	switch(menu->arg_type){

		case T_ARG_TYPE_START:
			#if CONFIG_DEVICE_SOLARKA
			if (menu_start_line) {
				fastProcessStart(&menu_start_wt_value, 100, 0, FP_PLUS, servo_fast_add);
			}
			else {
				fastProcessStart(&menu_start_period_value, 100, 0, FP_PLUS, servo_fast_add);
			}
			#elif CONFIG_DEVICE_SIEWNIK
			fastProcessStart(&servo_value, 100, 0, FP_PLUS, servo_fast_add);
			#endif
		break;

		default:
			return;
	}
	update_screen();
}

void button_plus_servo_up_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	debug_function_name("button_plus_servo_up_callback");
	switch(menu->arg_type)
	{
		
		case T_ARG_TYPE_START:
		#if CONFIG_DEVICE_SOLARKA
			fastProcessStop(&menu_start_wt_value);
			fastProcessStop(&menu_start_period_value);
		#elif CONFIG_DEVICE_SIEWNIK
			fastProcessStop(&servo_value);
		#endif
		save_parameters();
		break;

		default:
			return;
	}
	update_screen();
}

void button_minus_servo_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	debug_function_name("button_minus_servo_callback");
	switch(menu->arg_type)
	{
	
		case T_ARG_TYPE_START:
		#if CONFIG_DEVICE_SOLARKA
			if (menu_start_line) {
				if (menu_start_wt_value > 0) {
					menu_start_wt_value--;
					/* vibro value change */
					cmdClientSetValueWithoutRespI(MENU_VIBRO_WORKING_TIME, menu_start_wt_value);
				}
			}
			else {
				if (menu_start_period_value > 0) {
					menu_start_period_value--;
					/* vibro value change */
					cmdClientSetValueWithoutRespI(MENU_VIBRO_PERIOD, menu_start_period_value);
				}
			}
		#elif CONFIG_DEVICE_SIEWNIK
			if (servo_value > 0) {
				servo_value--;
				cmdClientSetValueWithoutRespI(MENU_SERVO, servo_value);
			}
		#endif
		break;

		default:
			return;
	}
	update_screen();
}

void button_minus_servo_time_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	debug_function_name("button_minus_servo_time_callback");
	switch(menu->arg_type){

		case T_ARG_TYPE_START:
			#if CONFIG_DEVICE_SOLARKA
			if (menu_start_line) {
				fastProcessStart(&menu_start_wt_value, 100, 0, FP_MINUS, servo_fast_add);
			}
			else {
				fastProcessStart(&menu_start_period_value, 100, 0, FP_MINUS, servo_fast_add);
			}
			#elif CONFIG_DEVICE_SIEWNIK
			fastProcessStart(&servo_value, 100, 0, FP_MINUS, servo_fast_add);
			#endif
		break;

		default:
			return;
	}
	update_screen();
}

void button_minus_servo_up_callback(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	switch(menu->arg_type)
	{
		
		case T_ARG_TYPE_START:
			#if CONFIG_DEVICE_SOLARKA
			fastProcessStop(&menu_start_wt_value);
			fastProcessStop(&menu_start_period_value);
			#elif CONFIG_DEVICE_SIEWNIK
			fastProcessStop(&servo_value);
			#endif
			save_parameters();
		break;

		default:
			return;
	}
	update_screen();
}

void button_motor_state(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	debug_function_name("button_motor_state");
	switch(menu->arg_type)
	{
		
		case T_ARG_TYPE_START:
			if (motor_on) {
				motor_on = 0;
				servo_vibro_on = 0;
				cmdClientSetValueWithoutRespI(MENU_MOTOR_IS_ON, motor_on); 
				cmdClientSetValueWithoutRespI(MENU_SERVO_IS_ON, servo_vibro_on);
				xTimerStop(xTimers, 0);
			}
			else {
				xTimerStart(xTimers, 0);
				motor_on = 1;
				cmdClientSetValueWithoutRespI(MENU_MOTOR_IS_ON, motor_on);
			}
		break;

		default:
			return;
	}
	update_screen();
}

void light_sleep(void)
{
	debug_msg("light_sleep\n\r");
	//esp_sleep_enable_gpio_wakeup();
	//gpio_wakeup_enable(WAKE_UP_PIN, GPIO_INTR_ANYEDGE);
	esp_sleep_enable_timer_wakeup(10 * 1000000);
	esp_wifi_stop();
	ssd1306_Fill(Black);
	ssd1306_UpdateScreen();
	BUZZER_OFF();
	esp_err_t ret = esp_light_sleep_start();
	debug_msg("light_sleep %d\n\r", ret);
}

void button_system_state(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == NULL) return;
	debug_function_name("button_system_state");
	if (is_sleeping())
	{
		if (menu->arg_type == T_ARG_TYPE_SLEEP)
		{
			remove_last_menu_tab();
		}
		go_to_wake_up();
		debug_msg("go_to_wake_up\n\r");
	}
	else
	{
		if (menu->arg_type != T_ARG_TYPE_SLEEP)
		{
			add_menu_tab(&sleep_menu);
		}
		go_to_sleep();
		debug_msg("go_to_sleep\n\r");
	}
	
	//light_sleep();

}

//toDo testowanie connect disconnect wifi
//menu backend and front-end
//nie dziala get all value

void button_parameters(void * arg)
{
	menu_token_t * menu = last_tab_element();
	if (menu == &parameters_menu)
	{
		menuActivateButtons();
		remove_last_menu_tab();
	}
}

void menuParametersButton(void)
{
	button1.fall_callback = button_parameters;
	button2.fall_callback = button_parameters;
	button6.fall_callback = button_parameters;
	button4.fall_callback = button_parameters;
	button7.fall_callback = button_parameters;
	button5.fall_callback = button_parameters;
	button3.fall_callback = button_parameters;
	button8.fall_callback = button_parameters;
	button9.fall_callback = button_parameters;
}

void menuDeactivateButtons(void)
{
	button1.fall_callback = NULL;
	button2.fall_callback = NULL;
	button3.fall_callback = NULL;
	button4.fall_callback = NULL;
	button4.timer_callback = NULL;
	button4.rise_callback = NULL;
	button5.fall_callback = NULL;
	button6.fall_callback = NULL;
	button6.timer_callback = NULL;
	button6.rise_callback = NULL;
	button7.fall_callback = NULL;
	button7.timer_callback = NULL;
	button7.rise_callback = NULL;
	button9.fall_callback = NULL;
	button10.fall_callback = NULL;
}

void menuActivateButtons(void)
{
	button1.fall_callback = button_up_callback;
	button2.fall_callback = button_down_callback;
	button6.fall_callback = button_plus_callback;
	button6.timer_callback = button_plus_time_callback;
	button6.rise_callback = button_plus_up_callback;
	button4.fall_callback = button_minus_callback;
	button4.timer_callback = button_minus_time_callback;
	button4.rise_callback = button_minus_up_callback;
	button7.fall_callback = button_plus_servo_callback;
	button7.timer_callback = button_plus_servo_time_callback;
	button7.rise_callback = button_plus_servo_up_callback;
	button5.fall_callback = button_minus_servo_callback;
	button5.timer_callback = button_minus_servo_time_callback;
	button5.rise_callback = button_minus_servo_up_callback;
	button3.fall_callback = button_enter_callback;
	button8.fall_callback = button_exit_callback;
	button9.fall_callback = button_motor_state;
	button10.fall_callback = button_system_state;
}

void menuPrintfInfo(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vsnprintf(infoBuff, sizeof(infoBuff), format, ap);
	va_end(ap);
	add_menu_tab(&info_menu);
}

void menuEnterParametersMenu(void)
{
	add_menu_tab(&parameters_menu);
	menuDeactivateButtons();
	menuParametersButton();
	update_screen();
	parameters_menu_flag = 0;
}

void init_menu(void)
{
	entered_menu_tab[0] = &main_menu;
	xSemaphore = xSemaphoreCreateBinary();
	osDelay(250);
	xTimers = xTimerCreate("Timer", MS2ST(2000), pdFALSE, ( void * ) 0, timerCallback);

	#if CONFIG_MENU_TEST_TASK
	xTaskCreate(menu_test, "menu_test", 4096, NULL, 12, NULL);
	#else
	xTaskCreate(menu_task, "menu_task", 4096, NULL, 12, NULL);
	#endif
	
	wifiDrvGetAPName(devName);
	if (connectToDevice(devName) == FALSE)
	{
		entered_menu_tab[1] = &wifi_menu;
		menu_start_find_device_I();
	}
	menuActivateButtons();
	update_screen();
}

static void menu_task(void * arg)
{
	(void) arg;
	menu_token_t * menu;
	while(1)
	{
		xSemaphoreTake( xSemaphore, ( TickType_t ) MS2ST(500) );
		taskENTER_CRITICAL();
		save_process();
		menu = last_tab_element();
		if (menu == NULL) 
		{
			debug_msg("MENU_ERROR: menu_task\n");
			taskEXIT_CRITICAL();
			vTaskDelay(MS2ST(100));
			continue;
		}
		ssd1306_Fill(Black);
		ssd1306_SetCursor(2, 0);
		ssd1306_WriteString(menu->name, Font_11x18, White);
		drawBattery(115, 1, battery_get_voltage());

		switch(menu->arg_type)
		{
			case T_ARG_TYPE_SLEEP:
				if (!is_sleeping())
				{
					ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
					ssd1306_WriteString("Go to sleep...", Font_7x10, White);
				}
				else 
				{
					ssd1306_Fill(Black);
				}
				break;

			case T_ARG_TYPE_BOOL:
				if (menu->value == NULL) 
				{
					debug_msg("T_ARG_TYPE_BOOL menu->value == NULL\n\r");
					continue;
				}
				ssd1306_SetCursor(50, MENU_HEIGHT + 5);
				if (*((bool*)menu->value) == 1)
					ssd1306_WriteString("ON", Font_11x18, White);
				else
					ssd1306_WriteString("OFF", Font_11x18, White);
				ssd1306_SetCursor(2, MENU_HEIGHT + 3*LINE_HEIGHT);
				ssd1306_WriteString("+/- For edit", Font_7x10, White);
			break;

			case T_ARG_TYPE_VALUE:
				if (menu->value == NULL) {
					debug_msg("T_ARG_TYPE_VALUE menu->value == NULL\n\r");
					continue;
				}
				{
					ssd1306_SetCursor(55, MENU_HEIGHT + 5);
					char buff[8];
					sprintf(buff, "%d", *menu->value);
					ssd1306_WriteString(buff, Font_11x18, White);
					ssd1306_SetCursor(2, MENU_HEIGHT + 3*LINE_HEIGHT - 1);
					ssd1306_WriteString("+/- For edit", Font_7x10, White);
				}
			break;

			case T_ARG_TYPE_MENU:
				if (menu->menu_list == NULL || menu->menu_list[menu->position] == NULL) {
					debug_msg("T_ARG_TYPE_MENU menu->value == NULL\n\r");
					continue;
				}
				{
					if (line_end - line_start != MAX_LINE - 1)
					{
						line_start = menu->position;
						line_end = line_start + MAX_LINE - 1;
					}
					if (menu->position < line_start || menu->position > line_end)
					{
						if (last_button)
						{
							line_start = menu->position;
							line_end = line_start + MAX_LINE - 1;
						}
						else
						{
							line_end = menu->position;
							line_start = line_end - MAX_LINE + 1;
						}
					}
					//debug_msg("line_start %d, line_end %d, position %d, last_button %d\n", line_start, line_end, menu->position, last_button);
					int line = 0;
					do
					{
						ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT*line);
						if (line + line_start == menu->position)
						{
							ssdFigureFillLine(MENU_HEIGHT + LINE_HEIGHT*line, LINE_HEIGHT);
							ssd1306_WriteString(menu->menu_list[line + line_start]->name, Font_7x10, Black);
						}
						else
						{
							ssd1306_WriteString(menu->menu_list[line + line_start]->name, Font_7x10, White);
						}
						
						line++;
					} while (menu->menu_list[line + line_start] != NULL && line < MAX_LINE);
					scrollBar.actual_line = menu->position;
					scrollBar.all_line = len_menu(menu) - 1;
					ssdFigureDrawScrollBar(&scrollBar);
				}
			break;

			case T_ARG_TYPE_PARAMETERS:
			{
				char param_buff[32];
				uint32_t voltage = 0;
				uint32_t temperature = 0;
				uint32_t current = 0;
				cmdClientGetValue(MENU_VOLTAGE_ACCUM, &voltage, 500);
				cmdClientGetValue(MENU_TEMPERATURE, &temperature, 500);
				cmdClientGetValue(MENU_CURRENT_MOTOR, &current, 500);
				sprintf(param_buff, "Voltage: %.2f [V]", (float)voltage / 100);
				ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
				ssd1306_WriteString(param_buff, Font_7x10, White);
				sprintf(param_buff, "Current: %.2f [A]", (float)current / 100);
				ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
				ssd1306_WriteString(param_buff, Font_7x10, White);
				sprintf(param_buff, "Temperature: %.2f [*C]", (float)temperature / 100);
				ssd1306_SetCursor(2, MENU_HEIGHT + 3*LINE_HEIGHT);
				ssd1306_WriteString(param_buff, Font_7x10, White);
			}
			break;

			case T_ARG_TYPE_START:
				if (!wifiDrvIsConnected()) {
					remove_last_menu_tab();
					if (wifiState == ST_WIFI_CONNECT) {
						menuPrintfInfo("Lost connection with target");
					}
					else {
						menuPrintfInfo("Target not connected. Go to DEVICES and connect to target");
					}
				}
				else if (menuGetValue(MENU_MOTOR_ERROR_IS_ON) == 1) {
					ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT*2);
					ssd1306_WriteString("MOTOR Error", Font_7x10, White);
				}
				else if (menuGetValue(MENU_SERVO_ERROR_IS_ON) == 1) {
					ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT*2);
					ssd1306_WriteString("SERVO Error", Font_7x10, White);
				}
				else { 
					if (animation_timeout < xTaskGetTickCount()) {
						animation_cnt++;
						animation_timeout = xTaskGetTickCount() + MS2ST(100);
					}
					char str[8];
					
					motor_bar.fill = motor_value;
					sprintf(str, "%d", motor_bar.fill);
					ssd1306_Fill(Black);
					ssdFigureDrawLoadBar(&motor_bar);
					ssd1306_SetCursor(80, 25);
					ssd1306_WriteString(str, Font_7x10, White);
					uint8_t cnt = 0;
					if (motor_on) {
						cnt = animation_cnt % 8;
					}
					if (cnt < 4) {
						if (cnt < 2) {
							drawMotor(2, 2 - cnt);
						}
						else {
							drawMotor(2, cnt - 2);
						}
					}
					else
					{
						if (cnt < 6) {
							drawMotor(2, cnt - 2);
						}
						else {
							drawMotor(2, 10 - cnt);
						}
					}
					#if CONFIG_DEVICE_SOLARKA

					/* PERIOD CURSOR */
					#define MENU_START_OFFSET 42
					char menu_buff[32];

					ssd1306_SetCursor(2, MENU_START_OFFSET);
					sprintf(menu_buff, "Period: %d [s]", menu_start_period_value);
					if (menu_start_line == 0)
					{
						ssdFigureFillLine(MENU_START_OFFSET, LINE_HEIGHT);
						ssd1306_WriteString(menu_buff, Font_7x10, Black);
					}
					else
					{
						ssd1306_WriteString(menu_buff, Font_7x10, White);
					}

					/* WORKING TIME CURSOR */
					sprintf(menu_buff, "Working time: %d [s]", menu_start_wt_value);
					ssd1306_SetCursor(2, MENU_START_OFFSET + LINE_HEIGHT);
					if (menu_start_line == 1)
					{
						ssdFigureFillLine(MENU_START_OFFSET + LINE_HEIGHT, LINE_HEIGHT);
						ssd1306_WriteString(menu_buff, Font_7x10, Black);
					}
					else
					{
						ssd1306_WriteString(menu_buff, Font_7x10, White);
					}
					#elif CONFIG_DEVICE_SIEWNIK
					servo_bar.fill = servo_value;
					sprintf(str, "%d", servo_bar.fill);
					ssdFigureDrawLoadBar(&servo_bar);
					ssd1306_SetCursor(80, 55);
					ssd1306_WriteString(str, Font_7x10, White);
					if (servo_vibro_on) {
						drawServo(10, 35, servo_value);
					}
					else {
						drawServo(10, 35, 0);
					}
					#endif
				}
				
			break;

			case T_ARG_TYPE_WIFI:
				if (wifiState == ST_WIFI_DEVICE_LIST)
				{
					if (dev_count == 0)
					{
						ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
						ssd1306_WriteString("Devices not found.", Font_7x10, White);
						ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
						ssd1306_WriteString("Click button for", Font_7x10, White);
						ssd1306_SetCursor(2, MENU_HEIGHT + 3*LINE_HEIGHT);
						ssd1306_WriteString("try find device", Font_7x10, White);
						break;
					}
					if (line_end - line_start != MAX_LINE - 1)
					{
						line_start = menu->position;
						line_end = line_start + MAX_LINE - 1;
					}
					if (menu->position < line_start || menu->position > line_end)
					{
						if (last_button)
						{
							line_start = menu->position;
							line_end = line_start + MAX_LINE - 1;
						}
						else
						{
							line_end = menu->position;
							line_start = line_end - MAX_LINE + 1;
						}
					}
					//debug_msg("position %d, dev_count %d line_start %d\n", menu->position, dev_count, line_start);
					int line = 0;
					do
					{
						ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT*line);
						if (line + line_start == menu->position)
						{
							ssdFigureFillLine(MENU_HEIGHT + LINE_HEIGHT*line, LINE_HEIGHT);
							ssd1306_WriteString(&wifi_device_list[line + line_start][6], Font_7x10, Black);
						}
						else
						{
							ssd1306_WriteString(&wifi_device_list[line + line_start][6], Font_7x10, White);
						}
						
						line++;
					} while (line + line_start < dev_count && line < MAX_LINE);
					scrollBar.actual_line = menu->position;
					scrollBar.all_line = dev_count - 1;
					ssdFigureDrawScrollBar(&scrollBar);
				}
				else if (wifiState == ST_WIFI_FIND_DEVICE)
				{
					ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
					ssd1306_WriteString("Scanning...", Font_7x10, White);
					break;
				}
				else if (wifiState == ST_WIFI_DEVICE_TRY_CONNECT)
				{
					if (wifiDrvIsConnected() == 1)
					{
						wifiState = ST_WIFI_CONNECT;
					}
					else
					{
						if (connect_timeout < xTaskGetTickCount())
						{
							wifiState = ST_WIFI_DEVICE_CONNECTED_FAILED;
						}
						ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
						ssd1306_WriteString("Try connect to", Font_7x10, White);
						ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
						ssd1306_WriteString(devName, Font_7x10, White);
					}
					
				}
				else if (wifiState == ST_WIFI_DEVICE_CONNECTED_FAILED)
				{
					ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT);
					ssd1306_WriteString("Connected failed.", Font_7x10, White);
					ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
					ssd1306_WriteString("Click button for", Font_7x10, White);
					ssd1306_SetCursor(2, MENU_HEIGHT + 3*LINE_HEIGHT);
					ssd1306_WriteString("try found device", Font_7x10, White);
				}
				else if (wifiState == ST_WIFI_CONNECT)
				{
					go_to_main_menu();
					menuPrintfInfo("Connected");
					break;
				}
			break;

			case T_ARG_TYPE_INFO:
				{
					int line = 0;
					ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT*line);
					for (int i = 0; i < strlen(infoBuff); i++)
					{
						if (i * 7 >= line * SSD1306_WIDTH + SSD1306_WIDTH)
						{
							line++;
							ssd1306_SetCursor(2, MENU_HEIGHT + LINE_HEIGHT*line);
						}
						ssd1306_WriteChar(infoBuff[i], Font_7x10, White);
					}
				}
				ssd1306_UpdateScreen();
				vTaskDelay(MS2ST(1500));
				menuActivateButtons();
				menu = last_tab_element();
				if (last_tab_element() == &info_menu)
					remove_last_menu_tab();
				break;

			default:
				{
					debug_msg("MENU switch default\n\r");
					continue;
				}
		}
		ssd1306_UpdateScreen();
		/* Update status led */
		MOTOR_LED_SET(motor_on);
		SERVO_VIBRO_LED_SET(servo_vibro_on);
		taskEXIT_CRITICAL();
	}
}

void menuEnterStartProcess(void) {
	servo_vibro_on = menuGetValue(MENU_SERVO_IS_ON);
	motor_on = menuGetValue(MENU_MOTOR_IS_ON);
	motor_value = menuGetValue(MENU_MOTOR);
	
	#if CONFIG_DEVICE_SOLARKA
	menu_start_wt_value = menuGetValue(MENU_VIBRO_WORKING_TIME);
	menu_start_period_value = menuGetValue(MENU_VIBRO_PERIOD);
	#endif

	#if CONFIG_DEVICE_SIEWNIK
	servo_value = menuGetValue(MENU_SERVO);
	#endif

	cmdClientSetValueWithoutRespI(MENU_START_SYSTEM, 1);
	cmdClientSetAllValue();
	update_screen();
}

void menuEnterStartFromServer(void) {
	go_to_main_menu();
	add_menu_tab(&start_menu);
	menuEnterStartProcess();
}

void menu_test(void * arg) 
{
	uint32_t task_cnt = 0;
	while (1)
	{
		if (wifiState != ST_WIFI_CONNECT) {
			switch(wifiState) {
				case  ST_WIFI_DEVICE_LIST:
				{
					if (dev_count == 0)
					{
						debug_msg("Device not found start scan\n\r");
						menu_start_find_device_I();
					}
					else {
						debug_msg("Try to connect device %s\n\r", wifi_device_list[0]);
						connectToDevice(wifi_device_list[0]);
					}
				}
				break;
				case ST_WIFI_DEVICE_TRY_CONNECT:
				{
					if (wifiDrvIsConnected() == 1)
					{
						debug_msg("Connected\n\r");
						wifiState = ST_WIFI_CONNECT;
					}
					else
					{
						if (connect_timeout < xTaskGetTickCount())
						{
							dev_count = 0;
							wifiState = ST_WIFI_DEVICE_LIST;
						}
					}
						
				}
				break;
				default:
					debug_msg("Default state\n\r");
				break;
			} //end switch
			vTaskDelay(250);
		}//end if
		
		if (cmdClientIsConnected() == 0) {
			debug_msg("Client not connected\n\r");
			vTaskDelay(1000);
		}
		else{
			if (task_cnt == 0) {
				vTaskDelay(3000);
				if (menuGetValue(MENU_START_SYSTEM) == 0) {
					debug_msg("MENU_START_SYSTEM is 0. Set 1\n\r");
					cmdClientSetValueWithoutResp(MENU_START_SYSTEM, 1);
				}
				else {
					debug_msg("MENU_START_SYSTEM is 1\n\r");
				}
			}
			cmdClientSetValueWithoutResp(MENU_MOTOR, motor_value++);
			cmdClientSetValueWithoutResp(MENU_VIBRO_PERIOD, motor_value++);
			if (motor_value > 99) {
				motor_value = 0;
			}
			vTaskDelay(2000);
			task_cnt++;
		}
	}
	
}


#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif