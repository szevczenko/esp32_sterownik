#ifndef MENU_DRV_H
#define MENU_DRV_H
#include "menu.h"

int menuDrvElementsCnt(menu_token_t * menu);
void menuEnter(menu_token_t * menu);
void menuExit(menu_token_t * menu);
void menuSetMain(menu_token_t * menu);
void menuDrvSaveParameters(void);
void menuPrintfInfo(const char *format, ...);
void menuDrvEnterEmergencyDisable(void);
void menuDrvExitEmergencyDisable(void);

#endif