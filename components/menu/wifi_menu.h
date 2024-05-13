#ifndef WIFI_MENU_H_
#define WIFI_MENU_H_
#include "menu_drv.h"

void menuInitWifiMenu( menu_token_t* menu );
void wifiMenu_SetDevType( const char* dev );
uint8_t wifiMenu_GetDevType( void );

#endif