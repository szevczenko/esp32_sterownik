#ifndef _VIBRO_H_
#define _VIBRO_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  VIBRO_STATE_NO_INIT,
  VIBRO_STATE_READY,
  VIBRO_STATE_CONFIGURED,
  VIBRO_STATE_START,
  VIBRO_STATE_STOP,
} vibro_state_t;

typedef enum
{
  VIBRO_TYPE_OFF,
  VIBRO_TYPE_ON,
} vibro_type_t;

typedef struct
{
  vibro_state_t state;
  vibro_type_t type;
#if MENU_VIRO_ON_OFF_VERSION
  uint32_t vibro_on_ms;
  uint32_t vibro_off_ms;
#else
  uint32_t period;
  uint32_t filling;
#endif
  uint32_t vibro_on_start_time;
  uint32_t vibro_off_start_time;
} vibro_t;

#if MENU_VIRO_ON_OFF_VERSION
void vibro_config( uint32_t vibro_on_ms, uint32_t vibro_off_ms );
#else
void vibro_config( uint32_t period, uint32_t filling );
#endif

void vibro_start( void );
void vibro_stop( void );
void vibro_init( void );
uint8_t vibro_is_on( void );
uint8_t vibro_is_started( void );

#endif