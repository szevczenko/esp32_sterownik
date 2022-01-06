#include <stdlib.h> // Required for libtelnet.h

#include <lwip/def.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include "telnet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cmd_client.h"

#include "freertos/queue.h"
#include "config.h"

#include "keepalive.h"
#include "parse_cmd.h"

#define LOG(...) debug_msg(__VA_ARGS__); \
	debug_msg("\n\r")

#define PAYLOAD_SIZE 128

enum cmd_client_app_state
{
    CMD_CLIENT_IDLE,
    CMD_CLIENT_CREATE_SOC,
    CMD_CLIENT_CONNECT_SERVER,
    CMD_CLIENT_STATE_READY,
    CMD_CLIENT_PARSE_RESPONSE,
    CMD_CLIENT_CLOSE_SOC,
    CMD_CLIENT_CHECK_ERRORS,
    CMD_CLIENT_TOP
};

static const char *cmd_client_state_name[] =
{
    [CMD_CLIENT_IDLE]              = "CMD_CLIENT_IDLE",
    [CMD_CLIENT_CREATE_SOC]        = "CMD_CLIENT_CREATE_SOC",
    [CMD_CLIENT_CONNECT_SERVER]    = "CMD_CLIENT_CONNECT_SERVER",
	[CMD_CLIENT_STATE_READY]	   = "CMD_CLIENT_STATE_READY",
    [CMD_CLIENT_PARSE_RESPONSE]    = "CMD_CLIENT_PARSE_RESPONSE",
    [CMD_CLIENT_CLOSE_SOC]         = "CMD_CLIENT_CLOSE_SOC",
    [CMD_CLIENT_CHECK_ERRORS]      = "CMD_CLIENT_CHECK_ERRORS",
};

/** @brief  CMD CL application context structure. */
struct cmd_client_context
{
    enum cmd_client_app_state state;
	struct sockaddr_in ip_addr;
	int socket;
	char cmd_ip_addr[16];
	uint32_t cmd_port;
    bool error;
    volatile bool start;
	volatile bool disconect_req;
    char payload[PAYLOAD_SIZE];
    size_t payload_size;
	uint8_t responce_buff[128];
	uint32_t responce_buff_len;
    keepAlive_t keepAlive;
	xSemaphoreHandle waitResponceSem;
	xSemaphoreHandle mutexSemaphore;
};

static struct cmd_client_context ctx;

static void _change_state(enum cmd_client_app_state new_state)
{
	debug_function_name(__func__);
	if (new_state < CMD_CLIENT_TOP)
	{
		ctx.state = new_state;
		LOG("Cmd Cl state: %s", cmd_client_state_name[new_state]);
	}
	else
	{
		LOG("Error change state cmd_cl: %d", new_state);
	}
}

/**
 * @brief   CMD Client application CMD_CLIENT_IDLE state.
 */
static void _idle_state(void)
{
	debug_function_name(__func__);
	if (ctx.disconect_req)
	{
		ctx.start = false;
		ctx.error = false;
		ctx.disconect_req = false;
	}

    if (ctx.start)
    {
        ctx.error = false;
        _change_state(CMD_CLIENT_CREATE_SOC);
    }
	else
	{
		osDelay(100);
	}
}

/**
 * @brief   CMD Client application CREATE_SOC state.
 */
static void _create_soc_state(void)
{
	debug_function_name(__func__);
	if (ctx.socket != -1)
	{
		_change_state(CMD_CLIENT_CLOSE_SOC);
		LOG("Cannot create socket");
		return;
	}

	ctx.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (ctx.socket >= 0)
    {
        _change_state(CMD_CLIENT_CONNECT_SERVER);
    }
    else
    {
        ctx.error = true;
        _change_state(CMD_CLIENT_CLOSE_SOC);
		LOG("error create sock");
    }
}

/**
 * @brief   CMD Client application CMD_CLIENT_CONNECT_SERVER state.
 */
static void _connect_server_state(void)
{
	debug_function_name(__func__);
	ctx.ip_addr.sin_addr.s_addr = inet_addr(ctx.cmd_ip_addr);
    ctx.ip_addr.sin_family = AF_INET;
    ctx.ip_addr.sin_port = htons( ctx.cmd_port );

	if (connect(ctx.socket, (struct sockaddr *)&ctx.ip_addr, sizeof(ctx.ip_addr)) < 0) 
	{
		LOG( "socket connect failed errno=%d \n", errno);
		ctx.error = true;
        _change_state(CMD_CLIENT_CLOSE_SOC);
        return;
	}
	keepAliveStart(&ctx.keepAlive);
	_change_state(CMD_CLIENT_STATE_READY);
}


static void _connect_ready_state(void)
{
	debug_function_name(__func__);
	int ret = 0;
	fd_set set;
	FD_ZERO(&set);
	FD_SET(ctx.socket, &set);
	struct timeval timeout_time;
	uint32_t timeout_ms = 1100;
	timeout_time.tv_sec=timeout_ms/1000;
	timeout_time.tv_usec =(timeout_ms%1000)*1000;

	if (!ctx.start || ctx.disconect_req)
	{
		_change_state(CMD_CLIENT_CLOSE_SOC);
	}

	ret = select(ctx.socket + 1, &set, NULL, NULL, &timeout_time);
	
	if (ret < 0) 
	{
		ctx.error = true;
        _change_state(CMD_CLIENT_CLOSE_SOC);
		LOG("error select errno %d", errno);
		return;
	}
	else if (ret == 0) 
	{
		return;
	}

	if (ctx.start && !ctx.disconect_req && FD_ISSET(ctx.socket , &set))
	{
		ret = read(ctx.socket, (char *)ctx.payload, PAYLOAD_SIZE);
		if (ret > 0)
		{
			ctx.payload_size = ret;
			_change_state(CMD_CLIENT_PARSE_RESPONSE);
		}
		else if (ret == 0)
		{
			LOG("error read 0 %d", ctx.socket);
			osDelay(50);
		}
		else
		{
			ctx.error = true;
        	_change_state(CMD_CLIENT_CLOSE_SOC);
			LOG("error read errno %d", errno);
		}
	}
	else
	{
		_change_state(CMD_CLIENT_CLOSE_SOC);
	}
}

static void _parse_response_state(void)
{
	debug_function_name(__func__);

    keepAliveAccept(&ctx.keepAlive);
	parse_client_buffer((uint8_t *)ctx.payload, ctx.payload_size);

	if (!ctx.start || ctx.disconect_req)
	{
		_change_state(CMD_CLIENT_CLOSE_SOC);
	}
	else
	{
		_change_state(CMD_CLIENT_STATE_READY);
	}
}

static void _close_soc_state(void)
{
	debug_function_name(__func__);
	if (ctx.socket != -1) {
		close(ctx.socket); 
		ctx.socket = -1;
	}
	keepAliveStop(&ctx.keepAlive);
    _change_state(CMD_CLIENT_CHECK_ERRORS);
}

static void _check_errors_state(void)
{
	debug_function_name(__func__);
    if (ctx.error)
    {
		LOG("Error detected");
    }

    ctx.error = false;
    _change_state(CMD_CLIENT_IDLE);
}

//--------------------------------------------------------------------------------

void cmd_client_task(void * arg)
{

    while(1)
    {
        switch (ctx.state)
        {
        case CMD_CLIENT_IDLE:
            _idle_state();
            break;

        case CMD_CLIENT_CREATE_SOC:
            _create_soc_state();
            break;

        case CMD_CLIENT_CONNECT_SERVER:
            _connect_server_state();
            break;

        case CMD_CLIENT_STATE_READY:
            _connect_ready_state();
            break;

        case CMD_CLIENT_PARSE_RESPONSE:
            _parse_response_state();
            break;

        case CMD_CLIENT_CLOSE_SOC:
            _close_soc_state();
            break;

        case CMD_CLIENT_CHECK_ERRORS:
            _check_errors_state();
            break;

        default:
            _change_state(CMD_CLIENT_IDLE);
            break;
        }
    }
}

void cmd_client_ctx_init(void)
{
    _change_state(CMD_CLIENT_IDLE);
    ctx.error = false;
    ctx.start = false;
	ctx.disconect_req = false;
	ctx.socket = -1;
    ctx.payload_size = 0;
	strcpy(ctx.cmd_ip_addr, "192.168.4.1");
	ctx.cmd_port = 8080;
}

bool cmd_client_is_iddle(void)
{
    return ctx.state == CMD_CLIENT_IDLE;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------

#define MAX_VALUE(OLD_V, NEW_VAL) NEW_VAL>OLD_V?NEW_VAL:OLD_V

int cmdClientSend(uint8_t* buffer, uint32_t len)
{
	debug_function_name("cmdClientSend");
	if (ctx.socket == -1 || (ctx.state != CMD_CLIENT_STATE_READY && ctx.state != CMD_CLIENT_PARSE_RESPONSE)) {
		return -1;
	}
	int ret = send(ctx.socket, buffer, len, 0);
	return ret;
}

int cmdClientSendDataWaitResp(uint8_t * buff, uint32_t len, uint8_t * buff_rx, uint32_t * rx_len, uint32_t timeout)
{
	debug_function_name("cmdClientSendDataWaitResp");
	if (buff[1] != CMD_REQEST)
		return FALSE;
	//debug_msg("cmdClientSendDataWaitResp start: %d\n\r",len);
	if (xSemaphoreTake(ctx.mutexSemaphore, timeout) == pdTRUE)
	{
		xQueueReset((QueueHandle_t )ctx.waitResponceSem);
		cmdClientSend(buff, len);
		if (xSemaphoreTake(ctx.waitResponceSem, timeout) == pdTRUE) 
		{
			if (buff[2] != ctx.responce_buff[2]) 
			{
				xSemaphoreGive(ctx.mutexSemaphore);
				memset(ctx.responce_buff, 0, sizeof(ctx.responce_buff));
				ctx.responce_buff_len = 0;
				debug_msg("cmdClientSendDataWaitResp end error: %d\n\r",len);
				return 0;
			}	

			if (rx_len != 0) 
			{
				debug_msg("cmdClientSendDataWaitResp answer len: %d\n\r",ctx.responce_buff_len);
				*rx_len = ctx.responce_buff_len;
			}

			if (buff_rx != 0) 
			{
				memcpy(buff_rx, ctx.responce_buff, ctx.responce_buff_len);
			}

			ctx.responce_buff_len = 0;
			xSemaphoreGive(ctx.mutexSemaphore);
			//debug_msg("cmdClientSendDataWaitResp end: %d\n\r",len);
			return 1;
		}
		else 
		{
			xSemaphoreGive(ctx.mutexSemaphore);
		}
	}
	debug_msg("Timeout cmdClientSendDataWaitResp\n");
	return FALSE;
}

int cmdClientSetValueWithoutResp(menuValue_t val, uint32_t value) 
{
	debug_function_name("cmdClientSetValueWithoutResp");
	if (menuSetValue(val, value) == FALSE){
		return FALSE;
	}
	uint8_t sendBuff[8];
	sendBuff[0] = 8;
	sendBuff[1] = CMD_DATA;
	sendBuff[2] = PC_SET;
	sendBuff[3] = val;
	memcpy(&sendBuff[4], (uint8_t *)&value, 4);

	return cmdClientSend(sendBuff, 8);
}

int cmdClientSetValueWithoutRespI(menuValue_t val, uint32_t value) 
{
	debug_function_name("cmdClientSetValueWithoutRespI");
	int ret_val = TRUE;
	if (menuSetValue(val, value) == FALSE){
		return FALSE;
	}
	uint8_t sendBuff[8];
	sendBuff[0] = 8;
	sendBuff[1] = CMD_DATA;
	sendBuff[2] = PC_SET;
	sendBuff[3] = val;
	memcpy(&sendBuff[4], (uint8_t *)&value, 4);

	taskEXIT_CRITICAL();
	ret_val = cmdClientSend(sendBuff, 8);
	taskENTER_CRITICAL();

	return ret_val;
}

int cmdClientGetValue(menuValue_t val, uint32_t * value, uint32_t timeout) 
{
	debug_function_name("cmdClientGetValue");
	if (val >= MENU_LAST_VALUE)
		return FALSE;

	if (xSemaphoreTake(ctx.mutexSemaphore, timeout) == pdTRUE)
	{
		uint8_t sendBuff[4];
		sendBuff[0] = 4;
		sendBuff[1] = CMD_REQEST;
		sendBuff[2] = PC_GET;
		sendBuff[3] = val;
		xQueueReset((QueueHandle_t )ctx.waitResponceSem);
		cmdClientSend(sendBuff, sizeof(sendBuff));
		if (xSemaphoreTake(ctx.waitResponceSem, timeout) == pdTRUE) {
			if (PC_GET != ctx.responce_buff[2]) {
				debug_msg("cmdClientGetValue error PC_GET != ctx.responce_buff[2]\n\r");
				xSemaphoreGive(ctx.mutexSemaphore);
				return 0;
			}	

			uint32_t return_value;

			memcpy(&return_value, &ctx.responce_buff[4], sizeof(return_value));

			if (menuSetValue(val, return_value) == FALSE) {
				debug_msg("cmdClientGetValue error val %d = %d\n\r", val, return_value);
				ctx.responce_buff_len = 0; 
				xSemaphoreGive(ctx.mutexSemaphore);
				return FALSE;
			}

			if (value != 0)
				*value = return_value;

			ctx.responce_buff_len = 0;
			xSemaphoreGive(ctx.mutexSemaphore);
			return 1;
		}
		else {
			xSemaphoreGive(ctx.mutexSemaphore);
			debug_msg("Timeout cmdClientGetValue\n");
		}
	}
	return FALSE;
}

int cmdClientSendCmd(parseCmd_t cmd) 
{
	debug_function_name("cmdClientSendCmd");
	int ret_val = TRUE;
	if (cmd > PC_CMD_LAST) { 
		return FALSE;
	}
	uint8_t sendBuff[3];
	sendBuff[0] = 3;
	sendBuff[1] = CMD_COMMAND;
	sendBuff[2] = cmd;

	ret_val = cmdClientSend(sendBuff, 3);

	return ret_val;
}

int cmdClientSendCmdI(parseCmd_t cmd) 
{
	debug_function_name("cmdClientSendCmdI");
	int ret_val = TRUE;
	if (cmd > PC_CMD_LAST) { 
		return FALSE;
	}
	uint8_t sendBuff[3];
	sendBuff[0] = 3;
	sendBuff[1] = CMD_COMMAND;
	sendBuff[2] = cmd;

	taskEXIT_CRITICAL();
	ret_val = cmdClientSend(sendBuff, 3);
	taskENTER_CRITICAL();

	return ret_val;
}

int cmdClientSetValue(menuValue_t val, uint32_t value, uint32_t timeout_ms) 
{
	debug_function_name("cmdClientSetValue");
	if (menuSetValue(val, value) == 0){
		return 0;
	}
	uint8_t sendBuff[8] = {0};
	uint8_t rxBuff[8] = {0};
	uint32_t reLen;
	sendBuff[0] = 8;
	sendBuff[1] = CMD_REQEST;
	sendBuff[2] = PC_SET;
	sendBuff[3] = val;
	memcpy(&sendBuff[4], (uint8_t *)&value, 4);

	if (cmdClientSendDataWaitResp(sendBuff, 8, rxBuff, &reLen, MS2ST(timeout_ms)) == 0) {
		return -1;
	}
	//debug_msg("cmdClientSetValue receive data %d\n", reLen);
	// for (uint8_t i = 0; i < reLen; i++) {
	// 	debug_msg("%d ", rxBuff[i]);
	// }
	// debug_msg("\n");
	if (rxBuff[3] == POSITIVE_RESP)
		return 1;

	return 0;
}

int cmdClientSetAllValue(void) 
{
	debug_function_name("cmdClientSetAllValue");
	void * data;
	uint32_t data_size;
	static uint8_t sendBuff[128];

	menuParamGetDataNSize(&data, &data_size);

	sendBuff[0] = 3 + data_size;
	sendBuff[1] = CMD_DATA;
	sendBuff[2] = PC_SET_ALL;
	memcpy(&sendBuff[3], data, data_size);

	cmdClientSend(sendBuff, 3 + data_size);

	return TRUE;
}

int cmdClientGetAllValue(uint32_t timeout) 
{
	debug_function_name("cmdClientGetAllValue");
	if (xSemaphoreTake(ctx.mutexSemaphore, timeout) == pdTRUE)
	{
		uint8_t sendBuff[3];

		sendBuff[0] = 3;
		sendBuff[1] = CMD_REQEST;
		sendBuff[2] = PC_GET_ALL;

		xQueueReset((QueueHandle_t )ctx.waitResponceSem);
		cmdClientSend(sendBuff, sizeof(sendBuff));
		if (xSemaphoreTake(ctx.waitResponceSem, timeout) == pdTRUE) {
			if (PC_GET_ALL != ctx.responce_buff[2]) {
				xSemaphoreGive(ctx.mutexSemaphore);
				return FALSE;
			}	
			
			uint32_t return_data;
			for (int i = 0; i < (ctx.responce_buff_len - 3) / 4; i++) {
				return_data = (uint32_t) ctx.responce_buff[3 + i*4];
				//debug_msg("VALUE: %d, %d\n\r", i, return_data);
				if (i == MENU_BOOTUP_SYSTEM || i == MENU_BUZZER || i == MENU_EMERGENCY_DISABLE)
				{
					continue;
				}
				
				if (menuSetValue(i, return_data) == FALSE) {
					debug_msg("Error Set Value %d = %d\n", i, return_data);
				}
			}

			ctx.responce_buff_len = 0;
			xSemaphoreGive(ctx.mutexSemaphore);
			return TRUE;
		}
		else {
			xSemaphoreGive(ctx.mutexSemaphore);
			debug_msg("Timeout cmdServerGetAllValue\n");
		}
	}
	return FALSE;
}

int cmdClientAnswerData(uint8_t * buff, uint32_t len) 
{
	debug_function_name("cmdClientAnswerData");
	if (buff == NULL)
		return FALSE;
	if (len > sizeof(ctx.responce_buff)) {
		debug_msg("cmdClientAnswerData error len %d\n", len);
		return FALSE;
	}
	//debug_msg("cmdClientAnswerData len %d %p\n", len, buff);
	ctx.responce_buff_len = len;
	memcpy(ctx.responce_buff, buff, ctx.responce_buff_len);

	xSemaphoreGiveFromISR(ctx.waitResponceSem, NULL);
	return TRUE;
}

static int keepAliveSend(uint8_t * data, uint32_t dataLen) 
{
	debug_function_name(__func__);
	return cmdClientSendDataWaitResp(data, dataLen, NULL, NULL, 1000);
}

static void cmdClientErrorKACb(void) 
{
	debug_msg("cmdClientErrorKACb keepAlive\n");
	cmdClientDisconnect();
}

void cmdClientStartTask(void)
{
	keepAliveInit(&ctx.keepAlive, 3800, keepAliveSend, cmdClientErrorKACb);
	ctx.waitResponceSem = xSemaphoreCreateBinary();
	ctx.mutexSemaphore = xSemaphoreCreateBinary();
	cmd_client_ctx_init();
	xSemaphoreGive(ctx.mutexSemaphore); 
	ctx.socket = -1;
	xTaskCreate(cmd_client_task, "cmd_client_task", 8192, NULL, NORMALPRIO, NULL);
}

void cmdClientStart(void)
{
	ctx.start = 1;
	ctx.disconect_req = false;
}

void cmdClientSetIp(char * ip)
{
	strcpy(ctx.cmd_ip_addr, ip);
}

void cmdClientSetPort(uint32_t port)
{
	ctx.cmd_port = port;
}

void cmdClientDisconnect(void)
{
	ctx.disconect_req = true;
}

int cmdClientIsConnected(void)
{
	return !ctx.disconect_req && ctx.start && (ctx.state == CMD_CLIENT_STATE_READY || ctx.state == CMD_CLIENT_PARSE_RESPONSE);
}

int cmdClientTryConnect(uint32_t timeout)
{
	ctx.disconect_req = 0;
	uint32_t time_now = ST2MS(xTaskGetTickCount());
	do 
	{
		if (cmdClientIsConnected())
		{
			return 1;
		}
		vTaskDelay(MS2ST(10));
	} while(time_now + timeout < ST2MS(xTaskGetTickCount()));

	if (cmdClientIsConnected())
	{
		return 1;
	}
	
	return 0;
}

void cmdClientStop(void)
{
	ctx.start = 0;
}
