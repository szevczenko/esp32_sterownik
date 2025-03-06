#ifndef MENU_AUTO_H
#define MENU_AUTO_H

#include "app_config.h"
#include "error_siewnik.h"
#include "error_solarka.h"
#include "menu_drv.h"

struct auto_data
{
  uint32_t velocity;
  uint32_t kg_per_ha;
  bool is_working;
};

void menuInitStartMenu( menu_token_t* menu );
void menuStartReset( void );
void menuStartSetError( error_type_t error );
void menuStartResetError( void );
struct menu_data* menuStartGetData( void );

#endif // MENU_AUTO_H
