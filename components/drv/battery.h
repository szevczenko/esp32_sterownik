#ifndef BATTERY_H
#define BATTERY_H

void battery_init(void);
float battery_get_voltage(void);
bool battery_get_charging_status(void);


#endif