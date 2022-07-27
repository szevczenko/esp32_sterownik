#ifndef _VIBRO_H_
#define _VIBRO_H_
typedef enum {
	VIBRO_STATE_NO_INIT,
	VIBRO_STATE_READY,
	VIBRO_STATE_CONFIGURED,
	VIBRO_STATE_START,
	VIBRO_STATE_STOP,
}vibro_state_t;

typedef enum {
	VIBRO_TYPE_OFF,
	VIBRO_TYPE_ON,
}vibro_type_t;

typedef struct {
	vibro_state_t state;
	vibro_type_t type;
	uint32_t vibro_on_ms;
	uint32_t vibro_off_ms;
}vibro_t;

void vibro_config(uint32_t period, uint32_t working_time);
void vibro_start (void);
void vibro_stop(void);
void vibro_init(void);
uint8_t vibro_is_on(void);
uint8_t vibro_is_started(void);

#endif