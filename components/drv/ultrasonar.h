#ifndef _ULTRASONAR_H_
#define _ULTRASONAR_H_
#include "config.h"
#include <stdbool.h>

void ultrasonar_start(void);
uint32_t ultrasonar_get_distance(void);
bool ultrasonar_is_connected(void);

#endif