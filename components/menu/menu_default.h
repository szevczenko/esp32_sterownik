#ifndef MENU_DEFAULT_H
#define MENU_DEFAULT_H
#include "menu_drv.h"

#define LINE_HEIGHT      11
#define MENU_HEIGHT      16
#define MAX_LINE         ( SSD1306_HEIGHT - MENU_HEIGHT ) / LINE_HEIGHT
#define NULL_ERROR_MSG() LOG( PRINT_ERROR, "Error menu pointer list is NULL (%s)\n\r", __func__ );

void menuInitDefaultList( menu_token_t* menu );
#endif