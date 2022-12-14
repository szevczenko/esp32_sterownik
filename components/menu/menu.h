#ifndef _MENU_H_
#define _MENU_H_
#include "stdint.h"
#include "dictionary.h"

#define LAST_BUTTON_UP 			true
#define LAST_BUTTON_DOWN 		false
typedef enum
{
	T_ARG_TYPE_BOOL,
	T_ARG_TYPE_VALUE,
	T_ARG_TYPE_MENU,
	T_ARG_TYPE_PARAMETERS
}menu_token_type_t;

typedef enum 
{
	MENU_DRV_NORMAL_INIT,
	MENU_DRV_LOW_BATTERY_INIT,
}menu_drv_init_t;

typedef struct
{
	void (*new_value)(uint32_t value);
	bool (*enter)(void * arg);
	bool (*exit)(void * arg);
	bool (*process)(void * arg);
	bool (*button_init_cb)(void * arg);
} menu_cb_t;

typedef struct
{
	void (*rise_callback)(void *button);
	void (*fall_callback)(void *button);
	void (*timer_callback)(void *button);
} menu_but_cb_t;

typedef struct
{
	uint8_t start;
	uint8_t end;
} menu_line_t;

typedef struct
{
	menu_but_cb_t up;
	menu_but_cb_t down;
	menu_but_cb_t enter;
	menu_but_cb_t exit;
	menu_but_cb_t up_minus;
	menu_but_cb_t up_plus;
	menu_but_cb_t down_minus;
	menu_but_cb_t down_plus;
	menu_but_cb_t on_off;
	menu_but_cb_t motor_on;
}menu_button_t;

typedef struct menu_token
{
	int token;
	enum dictionary_phrase name_dict;
	char *help;
	menu_token_type_t arg_type;
	struct menu_token **menu_list;
	uint32_t *value;
	
	/* For LCD position menu */
	uint8_t position;
	menu_line_t line; 
	bool last_button;
	bool update_screen_req;

	/* Menu callbacks */
	menu_cb_t menu_cb;
	menu_button_t button;
} menu_token_t;

extern uint32_t motor_value;
extern uint32_t servo_value;
extern bool motor_on;
extern bool servo_on;

void init_menu(menu_drv_init_t init_type);
void button_minus_callback(void * arg);
void menuDeactivateButtons(void);
void menuActivateButtons(void);
void menuPrintfInfo(const char *format, ...);
void menuEnterStartFromServer(void);

#endif