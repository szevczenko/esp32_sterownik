#ifndef START_MENU_H_
#define START_MENU_H_
#include "config.h"
#include "error_siewnik.h"

void menuInitStartMenu(menu_token_t *menu);
void menuStartReset(void);
void menuStartSetError(error_type_t error);
void menuStartResetError(void);

#endif