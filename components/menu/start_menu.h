#ifndef START_MENU_H_
#define START_MENU_H_
#include "config.h"
#include "error_siewnik.h"
#include "error_solarka.h"

struct menu_data
{
    uint32_t motor_value;
    uint32_t servo_value;
#if MENU_VIRO_ON_OFF_VERSION
    uint32_t vibro_off_s;
    uint32_t vibro_on_s;
#endif
    bool motor_on;
    bool servo_vibro_on;
};

void menuInitStartMenu(menu_token_t *menu);
void menuStartReset(void);
void menuStartSetError(error_type_t error);
void menuStartResetError(void);
struct menu_data *menuStartGetData(void);

#endif