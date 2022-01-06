#include "config.h"
#include "menu.h"
#include "menu_drv.h"
#include "ssd1306.h"
#include "ssdFigure.h"
#include "menu_default.h"
#include "freertos/timers.h"

#include "wifidrv.h"
#include "cmd_client.h"
#include "fast_add.h"
#include "battery.h"

#define DEVICE_LIST_SIZE 16
#define CHANGE_MENU_TIMEOUT_MS 1500
#define POWER_SAVE_TIMEOUT_MS 30 * 1000
#define CHANGE_VALUE_DISP_OFFSET 60
#define CHANGE_VALUE_DISP_OFFSET 60

typedef enum
{
	STATE_INIT,
	STATE_IDLE,
	STATE_START,
	STATE_READY,
	STATE_POWER_SAVE,
	STATE_ERROR,
	STATE_INFO,
	STATE_MOTOR_CHANGE,
	STATE_SERVO_VIBRO_CHANGE,
	STATE_STOP,
	STATE_ERROR_CHECK,
	STATE_TOP
} state_start_menu_t;

typedef enum
{
	EDIT_MOTOR,
	EDIT_SERVO,
	EDIT_WORKING_TIME,
	EDIT_PERIOD,
	EDIT_TOP
} edit_value_t;

typedef enum
{
	ERROR_NO_ERROR,
	ERROR_MOTOR_OVER_CURRENT,
	ERROR_MOTOR_OVER_TEMPERATURE,
	ERROR_SERVO_CANNOT_CLOSE,
	ERROR_SERVO_OVER_CURRENT,
	ERROR_OVER_TEMPERATURE,
	ERROR_TOP
} error_type_t;

typedef struct 
{
	volatile state_start_menu_t state;
	state_start_menu_t last_state;
	bool error_flag;
	int error_code;
	char * error_msg;
	char * info_msg;
	char buff[128];
	
	error_type_t error_dev;
	edit_value_t edit_value;
	uint32_t motor_value;
	uint32_t servo_value;
	uint32_t vibro_wt_value;
	uint32_t vibro_period_value;
	bool motor_on;
	bool servo_vibro_on;

	TickType_t animation_timeout;
	uint8_t animation_cnt;
	TickType_t change_menu_timeout;
	TickType_t go_to_power_save_timeout;
	TimerHandle_t servo_timer;

} menu_start_context_t;

static menu_start_context_t ctx;

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

static char *state_name[] = 
{
	[STATE_INIT] = "STATE_INIT",
	[STATE_IDLE] = "STATE_IDLE",
	[STATE_START] = "STATE_START",
	[STATE_READY] = "STATE_READY",
	[STATE_POWER_SAVE] = "STATE_POWER_SAVE",
	[STATE_ERROR] = "STATE_ERROR",
	[STATE_INFO] = "STATE_INFO",
	[STATE_MOTOR_CHANGE] = "STATE_MOTOR_CHANGE",
	[STATE_SERVO_VIBRO_CHANGE] = "STATE_SERVO_VIBRO_CHANGE",
	[STATE_STOP] = "STATE_STOP",
	[STATE_ERROR_CHECK] = "STATE_ERROR_CHECK",
};

static void change_state(state_start_menu_t new_state)
{
	debug_function_name(__func__);
	if (ctx.state < STATE_TOP)
	{
		if (ctx.state != new_state)
		{
			debug_msg("Start menu %s\n\r", state_name[new_state]);
		}
		ctx.state = new_state;
	}
	else
	{
		debug_msg("ERROR: change state %d\n\r", new_state);
	}
}

static void menu_show_info(char * info)
{
	debug_function_name(__func__);
	ctx.info_msg = info;
	ctx.last_state = ctx.state;
	menuPrintfInfo(info);
	change_state(STATE_INFO);
}

static void menu_error(error_type_t error)
{
	ctx.error_dev = error;
	change_state(STATE_ERROR);
}

static void set_change_menu(edit_value_t val)
{
	debug_function_name(__func__);
	if (ctx.state == STATE_READY || ctx.state == STATE_SERVO_VIBRO_CHANGE || ctx.state == STATE_MOTOR_CHANGE)
	{
		switch(val)
		{
			case EDIT_MOTOR:
				change_state(STATE_MOTOR_CHANGE);
				break;

			case EDIT_PERIOD:
			case EDIT_SERVO:
			case EDIT_WORKING_TIME:
				change_state(STATE_SERVO_VIBRO_CHANGE);
				break;

			default:
				return;
		}
		ctx.change_menu_timeout = MS2ST(CHANGE_MENU_TIMEOUT_MS) + xTaskGetTickCount();
	}
}

static bool _is_working_state(void)
{
	if((ctx.state == STATE_READY) || (ctx.state == STATE_SERVO_VIBRO_CHANGE) || (ctx.state == STATE_MOTOR_CHANGE))
	{
		return true;
	}
	return false;
}

static bool _is_power_save(void)
{
	if(ctx.state == STATE_POWER_SAVE)
	{
		return true;
	}
	return false;
}

static void _enter_power_save(void)
{
	wifiDrvPowerSave(true);
}

static void _exit_power_save(void)
{
	wifiDrvPowerSave(false);
}

static void _reset_power_save_timer(void)
{
	ctx.go_to_power_save_timeout = MS2ST(POWER_SAVE_TIMEOUT_MS) + xTaskGetTickCount();
}

static void menu_button_up_callback(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}
	ctx.edit_value = EDIT_WORKING_TIME;
}

static void menu_button_down_callback(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}
	ctx.edit_value = EDIT_PERIOD;
}

static void menu_button_exit_callback(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}
	menuExit(menu);
}

static void menu_button_servo_callback(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}	

	xTimerStop(ctx.servo_timer, 0);
	ctx.servo_vibro_on = ctx.servo_vibro_on ? false : true;
	cmdClientSetValueWithoutResp(MENU_SERVO_IS_ON, ctx.servo_vibro_on);
}

static void menu_button_motor_callback(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

	if (ctx.motor_on)
	{
		ctx.motor_on = false;
		ctx.servo_vibro_on = false;
		cmdClientSetValueWithoutResp(MENU_MOTOR_IS_ON, ctx.motor_on); 
		cmdClientSetValueWithoutResp(MENU_SERVO_IS_ON, ctx.servo_vibro_on);
		xTimerStop(ctx.servo_timer, 0);
	}
	else
	{
		ctx.motor_on = true;
		xTimerStart(ctx.servo_timer, 0);
		cmdClientSetValueWithoutResp(MENU_MOTOR_IS_ON, ctx.motor_on);
	}
}

static void motor_fast_add_cb(uint32_t value) {
	debug_function_name(__func__);
	(void) value;
	cmdClientSetValueWithoutResp(MENU_MOTOR, ctx.motor_value);
	set_change_menu(EDIT_MOTOR);
}

static void menu_button_motor_plus_push_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

	if (ctx.motor_value < 100) 
	{
		ctx.motor_value++;
		cmdClientSetValueWithoutResp(MENU_MOTOR, ctx.motor_value);
	}
	set_change_menu(EDIT_MOTOR);
}

static void menu_button_motor_plus_time_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}
	
	fastProcessStart(&ctx.motor_value, 100, 0, FP_PLUS, motor_fast_add_cb);
}

static void menu_button_motor_minus_push_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

	if (ctx.motor_value > 0) 
	{
		ctx.motor_value--;
		cmdClientSetValueWithoutResp(MENU_MOTOR, ctx.motor_value);
	}
	set_change_menu(EDIT_MOTOR);
}

static void menu_button_motor_minus_time_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

	fastProcessStart(&ctx.motor_value, 100, 0, FP_MINUS, motor_fast_add_cb);
}

static void menu_button_motor_p_m_pull_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}
	fastProcessStop(&ctx.motor_value);

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

	menuDrvSaveParameters();
}

/*-------------SERVO BUTTONS------------*/

static void servo_fast_add_cb(uint32_t value) 
{
	debug_function_name(__func__);
	(void) value;
	#if CONFIG_DEVICE_SOLARKA
	if (ctx.edit_value == EDIT_WORKING_TIME) {
		cmdClientSetValueWithoutResp(MENU_VIBRO_WORKING_TIME, ctx.vibro_wt_value);
	}
	else {
		cmdClientSetValueWithoutResp(MENU_VIBRO_PERIOD, ctx.vibro_period_value);
	}
	#elif CONFIG_DEVICE_SIEWNIK
	cmdClientSetValueWithoutResp(MENU_SERVO, ctx.servo_value);
	#endif
	set_change_menu(EDIT_SERVO);
}

static void menu_button_servo_plus_push_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

#if CONFIG_DEVICE_SOLARKA
	if (ctx.edit_value == EDIT_WORKING_TIME) {
		if (ctx.vibro_wt_value < 100) {
			ctx.vibro_wt_value++;
			/* vibro value change */
			cmdClientSetValueWithoutResp(MENU_VIBRO_WORKING_TIME, ctx.vibro_wt_value);
		}
	}
	else {
		if (ctx.vibro_period_value < 100) {
			ctx.vibro_period_value++;
			/* vibro value change */
			cmdClientSetValueWithoutResp(MENU_VIBRO_PERIOD, ctx.vibro_period_value);
		}
	}
#elif CONFIG_DEVICE_SIEWNIK
	if (ctx.servo_value < 100) {
		ctx.servo_value++;
		cmdClientSetValueWithoutResp(MENU_SERVO, ctx.servo_value);
	}
#endif
	set_change_menu(EDIT_SERVO);
}

static void menu_button_servo_plus_time_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

#if CONFIG_DEVICE_SOLARKA
	if (menu_start_line) {
		fastProcessStart(&ctx.vibro_wt_value, 100, 0, FP_PLUS, servo_fast_add_cb);
	}
	else {
		fastProcessStart(&ctx.vibro_period_value, 100, 0, FP_PLUS, servo_fast_add_cb);
	}
#elif CONFIG_DEVICE_SIEWNIK
	fastProcessStart(&ctx.servo_value, 100, 0, FP_PLUS, servo_fast_add_cb);
#endif
}

static void menu_button_servo_minus_push_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

	#if CONFIG_DEVICE_SOLARKA
	if (ctx.edit_value == EDIT_WORKING_TIME) {
		if (ctx.vibro_wt_value > 0) {
			ctx.vibro_wt_value--;
			/* vibro value change */
			cmdClientSetValueWithoutResp(MENU_VIBRO_WORKING_TIME, ctx.vibro_wt_value);
		}
	}
	else {
		if (ctx.vibro_period_value > 0) {
			ctx.vibro_period_value--;
			/* vibro value change */
			cmdClientSetValueWithoutResp(MENU_VIBRO_PERIOD, ctx.vibro_period_value);
		}
	}
#elif CONFIG_DEVICE_SIEWNIK
	if (ctx.servo_value > 0) {
		ctx.servo_value--;
		cmdClientSetValueWithoutResp(MENU_SERVO, ctx.servo_value);
	}
#endif
	set_change_menu(EDIT_SERVO);
}

static void menu_button_servo_minus_time_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

#if CONFIG_DEVICE_SOLARKA
	if (menu_start_line) {
		fastProcessStart(&ctx.vibro_wt_value, 100, 0, FP_MINUS, servo_fast_add_cb);
	}
	else {
		fastProcessStart(&ctx.vibro_period_value, 100, 0, FP_MINUS, servo_fast_add_cb);
	}
#elif CONFIG_DEVICE_SIEWNIK
	fastProcessStart(&ctx.servo_value, 100, 0, FP_MINUS, servo_fast_add_cb);
#endif
}

static void menu_button_servo_p_m_pull_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return;
	}
#if CONFIG_DEVICE_SOLARKA
	fastProcessStop(&ctx.vibro_wt_value);
	fastProcessStop(&ctx.vibro_period_value);
#elif CONFIG_DEVICE_SIEWNIK
	fastProcessStop(&ctx.servo_value);
#endif

	_reset_power_save_timer();

	if (_is_power_save())
	{
		_exit_power_save();
		change_state(STATE_READY);
	}

	if (!_is_working_state())
	{
		return;
	}

	menuDrvSaveParameters();
}

static bool menu_button_init_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}
	menu->button.down.fall_callback = menu_button_down_callback;
	menu->button.up.fall_callback = menu_button_up_callback;
	menu->button.enter.fall_callback = menu_button_exit_callback;
	menu->button.exit.fall_callback = menu_button_servo_callback;

	menu->button.up_minus.fall_callback = menu_button_motor_minus_push_cb;
	menu->button.up_minus.rise_callback = menu_button_motor_p_m_pull_cb;
	menu->button.up_minus.timer_callback = menu_button_motor_minus_time_cb;
	menu->button.up_plus.fall_callback = menu_button_motor_plus_push_cb;
	menu->button.up_plus.rise_callback = menu_button_motor_p_m_pull_cb;
	menu->button.up_plus.timer_callback = menu_button_motor_plus_time_cb;

	menu->button.down_minus.fall_callback = menu_button_servo_minus_push_cb;
	menu->button.down_minus.rise_callback = menu_button_servo_p_m_pull_cb;
	menu->button.down_minus.timer_callback = menu_button_servo_minus_time_cb;
	menu->button.down_plus.fall_callback = menu_button_servo_plus_push_cb;
	menu->button.down_plus.rise_callback = menu_button_servo_p_m_pull_cb;
	menu->button.down_plus.timer_callback = menu_button_servo_plus_time_cb;

	menu->button.motor_on.fall_callback = menu_button_motor_callback;
	return true;
}

static bool menu_enter_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}
	_exit_power_save();
	_reset_power_save_timer();
	cmdClientSetValueWithoutResp(MENU_START_SYSTEM, 1);
	ctx.error_flag = 0;
	return true;
}

static bool menu_exit_cb(void * arg)
{
	debug_function_name(__func__);
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}
	cmdClientSetValueWithoutResp(MENU_START_SYSTEM, 0);
	MOTOR_LED_SET(0);
	SERVO_VIBRO_LED_SET(0);
	return true;
}

static bool menu_is_connected(void)
{
	debug_function_name(__func__);
	if (!wifiDrvIsConnected())
	{
		ctx.error_flag = true;
		ctx.error_msg = "WiFi not connected";
		change_state(STATE_ERROR_CHECK);
		return false;
	}

	if (!cmdClientIsConnected())
	{
		ctx.error_flag = true;
		ctx.error_msg = "Client not connected";
		change_state(STATE_ERROR_CHECK);
		return false;
	}

	return true;
}

static void menu_start_init(void)
{
	ssd1306_SetCursor(2, MENU_HEIGHT + 2*LINE_HEIGHT);
	ssd1306_WriteString("Wait to read...", Font_7x10, White);
	ssd1306_UpdateScreen();
	if (cmdClientGetAllValue(2500) != TRUE)
	{
		debug_msg("%s: error get parameters\n\r", __func__);
	}
	change_state(STATE_IDLE);
}

static void menu_start_idle(void)
{
	if (menu_is_connected())
	{
		change_state(STATE_START);
	}
	else
	{
		menuPrintfInfo("Target not connected.\nGo to DEVICES\nfor connect");
	}
}

static void menu_start_start(void)
{
	if (!menu_is_connected())
	{
		return;
	}

	change_state(STATE_READY);
}

static void menu_start_ready(void)
{
	debug_function_name(__func__);
	if (!menu_is_connected())
	{
		ctx.error_msg = "Lost connection with server";
		return;
	}

	if (ctx.go_to_power_save_timeout < xTaskGetTickCount())
	{
		_enter_power_save();
		change_state(STATE_POWER_SAVE);
		return;
	}

	if (ctx.animation_timeout < xTaskGetTickCount()) {
		ctx.animation_cnt++;
		ctx.animation_timeout = xTaskGetTickCount() + MS2ST(100);
	}
	char str[8];
	
	motor_bar.fill = ctx.motor_value;
	sprintf(str, "%d", motor_bar.fill);
	ssd1306_Fill(Black);
	ssdFigureDrawLoadBar(&motor_bar);
	ssd1306_SetCursor(80, 25);
	ssd1306_WriteString(str, Font_7x10, White);
	uint8_t cnt = 0;

	if (ctx.motor_on) 
	{
		cnt = ctx.animation_cnt % 8;
	}
	if (cnt < 4) 
	{
		if (cnt < 2) 
		{
			drawMotor(2, 2 - cnt);
		}
		else 
		{
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
	servo_bar.fill = ctx.servo_value;
	sprintf(str, "%d", servo_bar.fill);
	ssdFigureDrawLoadBar(&servo_bar);
	ssd1306_SetCursor(80, 55);
	ssd1306_WriteString(str, Font_7x10, White);
	if (ctx.servo_vibro_on) 
	{
		drawServo(10, 35, ctx.servo_value);
	}
	else 
	{
		drawServo(10, 35, 0);
	}
	#endif
}

static void menu_start_power_save(void)
{
	if (!menu_is_connected())
	{
		ctx.error_msg = "Lost connection with server";
		return;
	}

	menuPrintfInfo("Power save\n\r");
}

static void menu_start_error(void)
{
	menuPrintfInfo("Error ocurred.\nClick any button\nto reset error");
}

static void menu_start_info(void)
{
	debug_function_name(__func__);
	osDelay(2500);
	change_state(ctx.last_state);
}

static void menu_start_motor_change(void)
{
	if (!menu_is_connected())
	{
		ctx.error_msg = "Lost connection with server";
		return;
	}

	ssd1306_SetCursor(2, 0);
	ssd1306_WriteString("MOTOR", Font_11x18, White);
	ssd1306_SetCursor(CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT);
	sprintf(ctx.buff, "%d", ctx.motor_value);
	ssd1306_WriteString(ctx.buff, Font_16x26, White);

	if (ctx.change_menu_timeout < xTaskGetTickCount())
	{
		change_state(STATE_READY);
	}
}

static void menu_start_vibro_change(void)
{
	debug_function_name(__func__);
	if (!menu_is_connected())
	{
		ctx.error_msg = "Lost connection with server";
		return;
	}

	ssd1306_SetCursor(2, 0);
	ssd1306_WriteString("SERVO", Font_11x18, White);
	ssd1306_SetCursor(CHANGE_VALUE_DISP_OFFSET, MENU_HEIGHT + LINE_HEIGHT);
	sprintf(ctx.buff, "%d", ctx.servo_value);
	ssd1306_WriteString(ctx.buff, Font_16x26, White);

	if (ctx.change_menu_timeout < xTaskGetTickCount())
	{
		change_state(STATE_READY);
	}
}

static void menu_start_stop(void)
{

}

static void menu_start_error_check(void)
{
	if (ctx.error_flag)
	{
		menuPrintfInfo(ctx.error_msg);
	}
	else
	{
		change_state(STATE_INIT);
	}
}

static bool menu_process(void * arg)
{
	menu_token_t *menu = arg;
	if (menu == NULL)
	{
		NULL_ERROR_MSG();
		return false;
	}

	switch(ctx.state)
	{
		case STATE_INIT:
			menu_start_init();
			break;

		case STATE_IDLE:
			menu_start_idle();
			break;

		case STATE_START:
			menu_start_start();
			break;

		case STATE_READY:
			menu_start_ready();
			break;

		case STATE_POWER_SAVE:
			menu_start_power_save();
			break;

		case STATE_ERROR:
			menu_start_error();
			break;

		case STATE_INFO:
			menu_start_info();
			break;

		case STATE_MOTOR_CHANGE:
			menu_start_motor_change();
			break;

		case STATE_SERVO_VIBRO_CHANGE:
			menu_start_vibro_change();
			break;

		case STATE_STOP:
			menu_start_stop();
			break;
		
		case STATE_ERROR_CHECK:
			menu_start_error_check();
			break;

		default:
			change_state(STATE_STOP);
			break;	
	}

	if (ctx.motor_on)
	{
		MOTOR_LED_SET(1);
	}
	else
	{
		MOTOR_LED_SET(0);
	}

	if (ctx.servo_vibro_on)
	{
		SERVO_VIBRO_LED_SET(1);
	}
	else
	{
		SERVO_VIBRO_LED_SET(0);
	}
	return true;
}

static void timerCallback(void * pv) 
{
	ctx.servo_vibro_on = true;
	cmdClientSetValueWithoutResp(MENU_SERVO_IS_ON, ctx.servo_vibro_on);
}

void menuInitStartMenu(menu_token_t *menu)
{
	memset(&ctx, 0, sizeof(ctx));
	menu->menu_cb.enter = menu_enter_cb;
	menu->menu_cb.button_init_cb = menu_button_init_cb;
	menu->menu_cb.exit = menu_exit_cb;
	menu->menu_cb.process = menu_process;
	ctx.servo_timer = xTimerCreate("servoTimer", MS2ST(2000), pdFALSE, ( void * ) 0, timerCallback);
}