#ifndef MENU_AUTO_H
#define MENU_AUTO_H

#include "app_config.h"
#include "error_siewnik.h"
#include "error_solarka.h"
#include "menu_drv.h"

struct auto_data
{
  uint32_t velocity;      // Read from machine
  uint32_t set_velocity;  // Edited in menu_auto and sent to machine
  uint32_t kg_per_ha;
  uint32_t motor_value;
  bool is_working;
};

void menuAutoInit( menu_token_t* menu );
void menuAutoReset( void );
void menuAutoSetError( error_type_t error );
void menuAutoResetError( void );
struct auto_data* menuAutoGetData( void );

#endif // MENU_AUTO_H
