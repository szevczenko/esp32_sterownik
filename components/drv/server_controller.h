#ifndef SERVER_CONTROLLER_H_
#define SERVER_CONTROLLER_H_
#include "config.h"

bool srvrControllGetMotorStatus(void);
bool srvrControllGetServoStatus(void);
uint8_t srvrControllGetMotorPwm(void);
uint16_t srvrControllGetServoPwm(void);
bool srvrControllGetEmergencyDisable(void);
void srvrControllStart(void);

#endif