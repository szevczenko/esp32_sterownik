/**
 * Test the telnet functions.
 *
 * Perform a test using the telnet functions.
 * This code exports two new global functions:
 *
 * void telnet_listenForClients(void (*callback)(uint8_t *buffer, size_t size))
 * void telnet_sendData(uint8_t *buffer, size_t size)
 *
 * For additional details and documentation see:
 * * Free book on ESP32 - https://leanpub.com/kolban-ESP32
 *
 *
 * Neil Kolban <kolban1@kolban.com>
 *
 */
#include <stdlib.h> // Required for libtelnet.h

#include "libtelnet.h"
#include <lwip/def.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include "telnet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "freertos/queue.h"
#include "config.h"

#include "wifidrv.h"

extern portMUX_TYPE portMux;

// The global tnHandle ... since we are only processing ONE telnet
// client at a time, this can be a global static.
telnet_t *tnHandle[TELNET_MAX_CLIENT];
telnet_server_p telnetServer;
struct telnetUserData pTelnetUserData[TELNET_MAX_CLIENT];
static char *error_str = "The clients limit has been exeeded. Server dropped connection.";

static TaskHandle_t thread_do_telnet;
static TaskHandle_t thread_listen_client;
static uint8_t status_telnet;
static uint8_t telnet_reset_flag;

static uint8_t buffer[TELNET_SEVER_BUFF_SIZE];
static const telnet_telopt_t my_telopts[] = {
    { TELNET_TELOPT_TTYPE,     TELNET_WONT, TELNET_DO },
	{ TELNET_TELOPT_SGA,	   TELNET_WILL, TELNET_DO },
    { TELNET_TELOPT_COMPRESS2, TELNET_WONT, TELNET_DO   },
    { TELNET_TELOPT_ZMP,       TELNET_WONT, TELNET_DO   },
    { TELNET_TELOPT_MSSP,      TELNET_WONT, TELNET_DO   },
    { TELNET_TELOPT_BINARY,    TELNET_WILL, TELNET_DO   },
    { TELNET_TELOPT_NAWS,      TELNET_WILL, TELNET_DONT },
    { -1, 0, 0 }
  };

static char read_char(struct telnetUserData* userData, char *symb)
{
	portENTER_CRITICAL(&portMux);
	uint8_t rev_val = ring_buffer_get(&userData->ringBuff, (void*)symb);//
	portEXIT_CRITICAL(&portMux);
	if (rev_val == 0)
	{
		return TRUE;
	}
	return FALSE;
}

static void telnetInitUserData(struct telnetUserData *userData, int socket)
{
	rb_attr_t telnetRxAttr;
	userData->sockfd = socket;
	telnetRxAttr.buffer = userData->buff;
	telnetRxAttr.n_elem = TELNET_CLIENT_BUFF_SIZE;
	telnetRxAttr.s_elem = sizeof(uint8_t);
	ring_buffer_init(&userData->ringBuff, &telnetRxAttr);
	userData->read_char = read_char;
}

void telnetInit(void)
{

}

/**
 * Telnet handler.
 */
static void telnetHandler(
		telnet_t *thisTelnet,
		telnet_event_t *event,
		void *userData) {
	int rc = 0;
	struct telnetUserData *telnetUserData = (struct telnetUserData *)userData;
	switch(event->type) {
	case TELNET_EV_SEND:
		rc = send(telnetUserData->sockfd, event->data.buffer, event->data.size, 0);
		if (rc < 0) {
		}
		break;

	case TELNET_EV_DATA:
		portENTER_CRITICAL(&portMux);
		for(size_t i = 0; i < event->data.size; i++)
		{
			if(ring_buffer_put(&telnetUserData->ringBuff, (void*) &event->data.buffer[i]) == -1)
			{
				break;
			}
		}
		portEXIT_CRITICAL(&portMux);
	default:
		break;
	} // End of switch event type
} // myTelnetHandler

static void delete_client(uint8_t number)
{
	portENTER_CRITICAL(&portMux);
	if (tnHandle[number] == NULL) return;
	telnetServer.client_count--;
  	telnet_free(tnHandle[number]);
 	tnHandle[number] = NULL;
	portEXIT_CRITICAL(&portMux);
	close(pTelnetUserData[number].sockfd);
}

static void doTelnet(void * pv) {
	printf(   "--> doTelnet\n");
	int rv, max_socket;
	struct timeval timeout_time;
	fd_set set;
	FD_ZERO(&set);
	telnetServer.client_count=0;
	while(1)
	{		
		timeout_time.tv_sec = 0;
		timeout_time.tv_usec = 100000;
		max_socket = 0;
		FD_ZERO(&set);
		if (telnet_reset_flag == 1)
		{
			for (uint8_t i = 0; i < TELNET_MAX_CLIENT; i++)
			{
				delete_client(i);
			}
			telnet_reset_flag = 0;
		}
		for (int i = 0; i<TELNET_MAX_CLIENT; i++)
		{
			if (tnHandle[i] == NULL) continue;
			FD_SET(pTelnetUserData[i].sockfd, &set);
			if (pTelnetUserData[i].sockfd > max_socket)
				max_socket = pTelnetUserData[i].sockfd;
		} 

		rv = select(max_socket + 1, &set, NULL, NULL, &timeout_time);

		if (rv<0)
	    {
		    printf(   "doTelnet select: %d (%s)\n", errno, strerror(errno)); // an error accured 
	    }
	    else if(rv == 0)
	    {
    	    continue;
	    }

		for (uint8_t i = 0; i < TELNET_MAX_CLIENT; i++)
		{
			if (tnHandle[i] == NULL) continue;
			if (!FD_ISSET(pTelnetUserData[i].sockfd , &set)) continue;
			int len = read(pTelnetUserData[i].sockfd, (char *)buffer, sizeof(buffer));
	 		if (len < 0)
			{		
				printf(   "doTelnet read %d: %d (%s)\n", pTelnetUserData[i].sockfd, errno, strerror(errno)); // an error accured
				delete_client(i);
  			}
			if (len == 0)
			{
				delete_client(i);
				printf(   "Telnet partner %d finished. Has %d clients\n",i, telnetServer.client_count);
			}
			if (len>0)
			{
				telnet_recv(tnHandle[i], (char *)buffer, len);
			}
		} // end for
	} 
} // doTelnet

/**
 * Telnet init listen clients.
 */
static void telnet_initListenClients(void)
{
	telnetServer.socket = -1;
	while(1)
	{
		if (telnetServer.socket < 0)
			telnetServer.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (telnetServer.socket < 0)
		{
			printf(   "server socket error: %d (%s)\n", errno, strerror(errno));
			vTaskDelay(500);
			continue;
		} 
		telnetServer.addr.sin_family = AF_INET;
		telnetServer.addr.sin_addr.s_addr = htonl(INADDR_ANY);
		telnetServer.addr.sin_port = htons(23);
		int flags = 1;

		flags = 1;
  		if (setsockopt(telnetServer.socket, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&flags, sizeof(flags))) { printf("ERROR: setsocketopt(), SO_KEEPIDLE"); };

 	 	flags = 1;
  		if (setsockopt(telnetServer.socket, IPPROTO_TCP, TCP_KEEPCNT, (void *)&flags, sizeof(flags))) { printf("ERROR: setsocketopt(), SO_KEEPCNT");};

  		flags = 1;
  		if (setsockopt(telnetServer.socket, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&flags, sizeof(flags))) { printf("ERROR: setsocketopt(), SO_KEEPINTVL"); };

		int rc = bind(telnetServer.socket, (struct sockaddr *)&telnetServer.addr, sizeof(telnetServer.addr));
		if (rc < 0) {
			printf("bind: %d (%s)\n", errno, strerror(errno));
			vTaskDelay(500);
			continue;
		}

		rc = listen(telnetServer.socket, 5);
		//printf(   "listen");
		if (rc < 0) {
			printf("listen: %d (%s)\n", errno, strerror(errno));
			vTaskDelay(500);
			continue;
		}
		else
		{
			break;
		}
		
	}
}

/**
 * Listen for telnet clients and handle them.
 */
static void telnet_listenForClients(void *arg) 
{
	telnetStop();
	while(1) {
		if (status_telnet == 0)
		{
			telnet_initListenClients();
			status_telnet = 1;
			printf( "was init listen telnet thd \n");
		}
		socklen_t len = sizeof(telnetServer.addr);
		fd_set set;
		struct timeval timeout;
		int rv;
		FD_ZERO(&set); /* clear the set */
		FD_SET(telnetServer.socket, &set); /* add our file descriptor to the set */

		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		rv = select(telnetServer.socket + 1, &set, NULL, NULL, &timeout);
		if (rv<0)
		{
			printf(   "listenForClients select: %d (%s)\n", errno, strerror(errno)); /* an error accured */
		}
		else if(rv == 0)
		{
    		// printf(  "timeout occurred (10 second) \n"); /* a timeout occured */
		}
		else 
		{
			int partnerSocket = accept(telnetServer.socket, (struct sockaddr *)&telnetServer.addr, &len);
			if (partnerSocket < 0) {
				printf(   "accept: %d (%s)\n", errno, strerror(errno));
				continue;
			}
			printf(   "We have a new client connection! %d\n", partnerSocket);
			if (telnetServer.client_count<TELNET_MAX_CLIENT)
			{
				
				for (int i = 0 ; i < TELNET_MAX_CLIENT; i++)
				{
					if (tnHandle[i] == NULL)
					{
						portENTER_CRITICAL(&portMux);
						telnetInitUserData(&pTelnetUserData[i], partnerSocket);
						tnHandle[i] = telnet_init(my_telopts, telnetHandler, 0, &pTelnetUserData[i]);
						portEXIT_CRITICAL(&portMux);
						telnet_negotiate(tnHandle[i], TELNET_WILL, TELNET_TELOPT_ECHO);
						telnetServer.client_count++;
						break;
					}
				}
			}
			else
			{
				send(partnerSocket, error_str, strlen(error_str), 0);
				osDelay(500);
				close(partnerSocket);
			}
		}
	}
		
} // listenForNewClient

static void telnetListenTask(void *data) {

	telnet_listenForClients(NULL);
	vTaskDelete(NULL);
}

void telnetStartTask(void)
{
	xTaskCreate(doTelnet, "doTelnet", CONFIG_DO_TELNET_THD_WA_SIZE, NULL, NORMALPRIO, &thread_do_telnet);
	xTaskCreate(telnetListenTask, "telnetListenTask", CONFIG_TELNET_LISTEN_THD_WA_SIZE, NULL, NORMALPRIO, &thread_listen_client);
}

void telnetStart(void)
{
	status_telnet = 0;
	vTaskResume(thread_do_telnet);
	vTaskResume(thread_listen_client);
}

void telnetStop(void)
{
	close(telnetServer.socket);
	vTaskSuspend(thread_do_telnet);
	vTaskSuspend(thread_listen_client);
}

void telnetReset(void)
{
	telnet_reset_flag = 1;
}

void telnetSendToAll(const char * data, size_t size)
{
	for (uint8_t i = 0; i < TELNET_MAX_CLIENT; i++)
	{
		if (tnHandle[i] != 0)
		{
			telnet_send(tnHandle[i], data, size);
		}
	}
}

void telnetPrintfToAll(const char *format, ...)
{
	static char buff[CONFIG_CONSOLE_VSNPRINTF_BUFF_SIZE];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buff, CONFIG_CONSOLE_VSNPRINTF_BUFF_SIZE, format, ap);
	va_end (ap);
	for (uint8_t i = 0; i < TELNET_MAX_CLIENT; i++)
	{
		if (tnHandle[i] != 0)
		{
			telnet_send(tnHandle[i], buff, strlen(buff));
		}
	}
}
