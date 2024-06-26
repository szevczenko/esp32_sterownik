#ifndef SERVER_CONTROLLER_H_
#define SERVER_CONTROLLER_H_
#include "app_config.h"
#include "parse_cmd.h"

bool srvrControllGetMotorStatus( void );
bool srvrControllGetServoStatus( void );
uint8_t srvrControllGetMotorPwm( void );
uint16_t srvrControllGetServoPwm( void );
bool srvrControllGetEmergencyDisable( void );
void srvrControllStart( void );
bool srvrConrollerSetError( uint16_t error_reason );
bool srvrControllerErrorReset( void );
bool srvrControllIsWorking( void );

#endif