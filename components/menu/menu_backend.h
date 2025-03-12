#ifndef MENU_BACKEND_H_
#define MENU_BACKEND_H_
#include <stdbool.h>

void menuBackendInit( void );
void backendEnterMenuParameters( void );
void backendExitMenuParameters( void );
void backendToggleEmergencyDisable( void );
void backendEnterMenuStart( void );
void backendExitMenuStart( void );
void backendEnterMenuAuto( void );
void backendExitMenuAuto( void );
bool backendIsConnected( void );
bool backendIsEmergencyDisable( void );

#endif