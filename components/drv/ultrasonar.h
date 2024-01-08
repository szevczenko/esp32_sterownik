#ifndef _ULTRASONAR_H_
#define _ULTRASONAR_H_
#include <stdbool.h>

#include "config.h"

void ultrasonar_start( void );
uint32_t ultrasonar_get_distance( void );
bool ultrasonar_is_connected( void );

#endif