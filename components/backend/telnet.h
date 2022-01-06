#ifndef _TELNET_H
#define _TELNET_H
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "libtelnet.h"
#include <lwip/def.h>
#include <lwip/sockets.h>
#include "config.h"

#include "ringBuff.h"


#ifndef CONFIG_TELNET_LISTEN_THD_WA_SIZE
#define CONFIG_TELNET_LISTEN_THD_WA_SIZE 4096
#endif

#ifndef CONFIG_DO_TELNET_THD_WA_SIZE
#define CONFIG_DO_TELNET_THD_WA_SIZE 4096
#endif

#ifndef TELNET_MAX_CLIENT
#define TELNET_MAX_CLIENT 1
#endif

#ifndef TELNET_CLIENT_BUFF_SIZE
#define TELNET_CLIENT_BUFF_SIZE 128
#endif

#ifndef TELNET_SEVER_BUFF_SIZE
#define TELNET_SEVER_BUFF_SIZE 128
#endif

#if (CONFIG_USE_TCPIP && CONFIG_USE_CONSOLE_TELNET)
typedef struct
{
	int socket;
	struct sockaddr_in addr;
	uint8_t client_count;

}telnet_server_p;

struct telnetUserData {
	int sockfd;
	ring_buffer_t ringBuff;
	uint8_t buff[TELNET_CLIENT_BUFF_SIZE];
	char (*read_char)(struct telnetUserData *, char *);
};

enum telnet_state_t {
	TELNET_STATE_DATA = 0,
	TELNET_STATE_EOL,
	TELNET_STATE_IAC,
	TELNET_STATE_WILL,
	TELNET_STATE_WONT,
	TELNET_STATE_DO,
	TELNET_STATE_DONT,
	TELNET_STATE_SB,
	TELNET_STATE_SB_DATA,
	TELNET_STATE_SB_DATA_IAC
};
typedef enum telnet_state_t telnet_state_t;

typedef struct 
{
    /* user data */
	void *ud;
	/* telopt support table */
	const telnet_telopt_t *telopts;
	/* event handler */
	telnet_event_handler_t eh;
#if defined(HAVE_ZLIB)
	/* zlib (mccp2) compression */
	z_stream *z;
#endif
	/* RFC1143 option negotiation states */
	struct telnet_rfc1143_t *q;
	/* sub-request buffer */
	char *buffer;
	/* current size of the buffer */
	size_t buffer_size;
	/* current buffer write position (also length of buffer data) */
	size_t buffer_pos;
	/* current state */
	enum telnet_state_t state;
	/* option flags */
	unsigned char flags;
	/* current subnegotiation telopt */
	unsigned char sb_telopt;
	/* length of RFC1143 queue */
	unsigned char q_size;
}telnet_w_t;

void telnetInit(void);
void telnetStartTask(void);
void telnetStart(void);
void telnetStop(void);
void telnetReset(void);
void telnetSendToAll(const char * data, size_t size);

extern telnet_t *tnHandle[TELNET_MAX_CLIENT];
extern telnet_server_p telnetServer;
extern struct telnetUserData pTelnetUserData[TELNET_MAX_CLIENT];

#endif

#endif