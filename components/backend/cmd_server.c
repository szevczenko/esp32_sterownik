#include <stdlib.h> // Required for libtelnet.h

#include <lwip/def.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include "telnet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cmd_server.h"

#include "freertos/queue.h"
#include "config.h"

#include "parse_cmd.h"
#include "configCmd.h"
#include "menu_param.h"

#define MODULE_NAME     "[CMD Srv] "
#define DEBUG_LVL       PRINT_WARNING

#if CONFIG_DEBUG_CMD_SERVER
#define LOG(_lvl, ...) \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

#define MAX_VALUE(OLD_V, NEW_VAL)    NEW_VAL > OLD_V ? NEW_VAL : OLD_V
#define PAYLOAD_SIZE    256

enum state_t
{
    CMD_SERVER_IDLE = 0,
    CMD_SERVER_CREATE_SOC,
    CMD_SERVER_LISTEN,
    CMD_SERVER_STATE_READY,
    CMD_SERVER_PARSE_RESPONSE,
    CMD_SERVER_CLOSE_SOC,
    CMD_SERVER_CHECK_ERRORS,
    CMD_SERVER_TOP,
};

static const char *cmd_server_state_name[] =
{
    [CMD_SERVER_IDLE] = "CMD_SERVER_IDLE",
    [CMD_SERVER_CREATE_SOC] = "CMD_SERVER_CREATE_SOC",
    [CMD_SERVER_LISTEN] = "CMD_SERVER_LISTEN",
    [CMD_SERVER_STATE_READY] = "CMD_SERVER_STATE_READY",
    [CMD_SERVER_PARSE_RESPONSE] = "CMD_SERVER_PARSE_RESPONSE",
    [CMD_SERVER_CLOSE_SOC] = "CMD_SERVER_CLOSE_SOC",
    [CMD_SERVER_CHECK_ERRORS] = "CMD_SERVER_CHECK_ERRORS",
};

typedef struct
{
    enum state_t state;
    struct sockaddr_in ip_addr;
    int socket;
    struct sockaddr_in servaddr;
    int client_socket;
    bool disconnect_req;
    bool error;
    volatile bool start;
    char payload[PAYLOAD_SIZE];
    size_t payload_size;
    uint8_t responce_buff[PAYLOAD_SIZE];
    uint32_t responce_buff_len;
    keepAlive_t keepAlive;
    xSemaphoreHandle waitResponceSem;
    xSemaphoreHandle mutexSemaphore;
    TaskHandle_t thread_task_handle;

    bool is_connected;
} cmd_server_t;

static cmd_server_t ctx;

extern portMUX_TYPE portMux;

static void _change_state(enum state_t new_state)
{
    LOG(PRINT_INFO, "State: %s", cmd_server_state_name[new_state]);
    osDelay(10);
    ctx.state = new_state;
}

/**
 * @brief   CMD Server application CMD_SERVER_IDLE state.
 */
static void _idle_state(void)
{
    if (ctx.start)
    {
        ctx.error = false;
        ctx.disconnect_req = false;
        _change_state(CMD_SERVER_CREATE_SOC);
    }
    else
    {
        osDelay(100);
    }
}

/**
 * @brief   CMD Server application CREATE_SOC state.
 */
static void _create_soc_state(void)
{
    if (ctx.socket != -1)
    {
        _change_state(CMD_SERVER_CLOSE_SOC);
        LOG(PRINT_ERROR, "Cannot create socket");
        return;
    }

    ctx.socket = socket(AF_INET, SOCK_STREAM, 0);

    if (ctx.socket < 0)
    {
        ctx.error = true;
        _change_state(CMD_SERVER_CLOSE_SOC);
        LOG(PRINT_ERROR, "error create sock");
    }

    ctx.servaddr.sin_family = AF_INET;
    ctx.servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ctx.servaddr.sin_port = htons(PORT);

    int optval = 1;

    setsockopt(ctx.socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

    int rc = bind(ctx.socket, (struct sockaddr *)&ctx.servaddr, sizeof(ctx.servaddr));

    if (rc < 0)
    {
        LOG(PRINT_ERROR, "bind error %d (%s)", errno, strerror(errno));
        ctx.error = true;
        _change_state(CMD_SERVER_CLOSE_SOC);
        return;
    }

    rc = listen(ctx.socket, 5);
    if (rc < 0)
    {
        LOG(PRINT_ERROR, "listen: %d (%s)", errno, strerror(errno));
        ctx.error = true;
        _change_state(CMD_SERVER_CLOSE_SOC);
        return;
    }

    _change_state(CMD_SERVER_LISTEN);
}

/**
 * @brief   CMD Server application CMD_SERVER_CONNECT_SERVER state.
 */
static void _listen_client(void)
{
    int ret = 0;
    fd_set set;

    FD_ZERO(&set);
    FD_SET(ctx.socket, &set);
    struct timeval timeout_time;
    uint32_t timeout_ms = 1100;

    timeout_time.tv_sec = timeout_ms / 1000;
    timeout_time.tv_usec = (timeout_ms % 1000) * 1000;

    if (!ctx.start || ctx.disconnect_req)
    {
        _change_state(CMD_SERVER_CLOSE_SOC);
        return;
    }

    ret = select(ctx.socket + 1, &set, NULL, NULL, &timeout_time);

    if (ret < 0)
    {
        ctx.error = true;
        _change_state(CMD_SERVER_CLOSE_SOC);
        LOG(PRINT_ERROR, "error select errno %d", errno);
        return;
    }
    else if (ret == 0)
    {
        return;
    }

    socklen_t len = sizeof(ctx.servaddr);

    ret = accept(ctx.socket, (struct sockaddr *)&ctx.servaddr, &len);
    if (ret < 0)
    {
        LOG(PRINT_ERROR, "accept: %d (%s)", errno, strerror(errno));
        ctx.error = true;
        _change_state(CMD_SERVER_CLOSE_SOC);
        return;
    }

    ctx.client_socket = ret;
    keepAliveStart(&ctx.keepAlive);
    ctx.is_connected = true;
    LOG(PRINT_INFO, "We have a new client connection! %d", ctx.client_socket);

    _change_state(CMD_SERVER_STATE_READY);
}

static void _connect_ready_state(void)
{
    int ret = 0;
    fd_set set;

    FD_ZERO(&set);
    FD_SET(ctx.client_socket, &set);
    struct timeval timeout_time;
    uint32_t timeout_ms = 1100;

    timeout_time.tv_sec = timeout_ms / 1000;
    timeout_time.tv_usec = (timeout_ms % 1000) * 1000;

    if (!ctx.start || ctx.disconnect_req)
    {
        _change_state(CMD_SERVER_CLOSE_SOC);
        return;
    }

    ret = select(ctx.client_socket + 1, &set, NULL, NULL, &timeout_time);

    if (ret < 0)
    {
        ctx.error = true;
        _change_state(CMD_SERVER_CLOSE_SOC);
        LOG(PRINT_ERROR, "error select errno %d", errno);
        return;
    }
    else if (ret == 0)
    {
        return;
    }

    if (ctx.start && FD_ISSET(ctx.client_socket, &set))
    {
        ret = read(ctx.client_socket, (char *)ctx.payload, PAYLOAD_SIZE);
        if (ret > 0)
        {
            ctx.payload_size = ret;
            _change_state(CMD_SERVER_PARSE_RESPONSE);
        }
        else if (ret == 0)
        {
            LOG(PRINT_ERROR, "Server disconnected 0 %d", ctx.client_socket);
            _change_state(CMD_SERVER_CLOSE_SOC);
            osDelay(50);
        }
        else
        {
            ctx.error = true;
            _change_state(CMD_SERVER_CLOSE_SOC);
            LOG(PRINT_ERROR, "error read errno %d", errno);
        }
    }
    else
    {
        _change_state(CMD_SERVER_CLOSE_SOC);
    }
}

static void _parse_response_state(void)
{
    keepAliveAccept(&ctx.keepAlive);
    parse_server_buffer((uint8_t *)ctx.payload, ctx.payload_size);

    if (!ctx.start || ctx.disconnect_req)
    {
        _change_state(CMD_SERVER_CLOSE_SOC);
    }
    else
    {
        _change_state(CMD_SERVER_STATE_READY);
    }
}

static void _close_soc_state(void)
{
    if (ctx.client_socket != -1)
    {
        close(ctx.client_socket);
        ctx.client_socket = -1;
    }

    osDelay(50);
    if (ctx.socket != -1)
    {
        close(ctx.socket);
        ctx.socket = -1;
    }

    //keepAliveStop(&ctx.keepAlive);
    _change_state(CMD_SERVER_CHECK_ERRORS);
}

static void _check_errors_state(void)
{
    if (ctx.error)
    {
        LOG(PRINT_INFO, "Error detected");
    }

    ctx.error = false;
    ctx.disconnect_req = false;
    _change_state(CMD_SERVER_IDLE);
}

//--------------------------------------------------------------------------------

void cmd_server_task(void *arg)
{
    while (1)
    {
        switch (ctx.state)
        {
        case CMD_SERVER_IDLE:
            _idle_state();
            break;

        case CMD_SERVER_CREATE_SOC:
            _create_soc_state();
            break;

        case CMD_SERVER_LISTEN:
            _listen_client();
            break;

        case CMD_SERVER_STATE_READY:
            _connect_ready_state();
            break;

        case CMD_SERVER_PARSE_RESPONSE:
            _parse_response_state();
            break;

        case CMD_SERVER_CLOSE_SOC:
            _close_soc_state();
            break;

        case CMD_SERVER_CHECK_ERRORS:
            _check_errors_state();
            break;

        default:
            _change_state(CMD_SERVER_IDLE);
            break;
        }
    }
}

static void cmd_server_ctx_init(void)
{
    ctx.state = CMD_SERVER_IDLE;
    ctx.error = false;
    ctx.start = false;
    ctx.disconnect_req = false;
    ctx.socket = -1;
    ctx.client_socket = -1;
    ctx.payload_size = 0;
}

int cmdServerSendData(uint8_t *buff, uint8_t len)
{
    if ((ctx.socket == -1) || ((ctx.state != CMD_SERVER_STATE_READY) && (ctx.state != CMD_SERVER_PARSE_RESPONSE)))
    {
        LOG(PRINT_ERROR, "%s bad state", __func__);
        return -1;
    }

    int ret = send(ctx.client_socket, buff, len, 0);

    if (ret < 0)
    {
        LOG(PRINT_ERROR, "%s error send msg", __func__);
    }

    return ret;
}

int cmdServerSendDataWaitResp(uint8_t *buff, uint32_t len, uint8_t *buff_rx, uint32_t *rx_len, uint32_t timeout)
{
    if (buff[1] != CMD_REQEST)
    {
        return FALSE;
    }

    if (xSemaphoreTake(ctx.mutexSemaphore, timeout) == pdTRUE)
    {
        xQueueReset((QueueHandle_t)ctx.waitResponceSem);

        if (cmdServerSendData(buff, len) < 0)
        {
            LOG(PRINT_ERROR, "%s error send msg", __func__);
            xSemaphoreGive(ctx.mutexSemaphore);
            return FALSE;
        }

        if (xSemaphoreTake(ctx.waitResponceSem, timeout) == pdTRUE)
        {
            if (buff[2] != ctx.responce_buff[2])
            {
                xSemaphoreGive(ctx.mutexSemaphore);
                LOG(PRINT_ERROR, "%s end error: %d", __func__, len);
                return FALSE;
            }

            if ((buff_rx != 0) && (ctx.responce_buff_len <= sizeof(ctx.responce_buff)))
            {
                memcpy(buff_rx, ctx.responce_buff, ctx.responce_buff_len);
            }
            else
            {
                if (buff_rx != 0)
                {
                    LOG(PRINT_ERROR, "%s buffer is small responce_buff_len %d", __func__, ctx.responce_buff_len);
                }
            }

            if (rx_len != NULL)
            {
                LOG(PRINT_INFO, "%s answer len: %d", __func__, ctx.responce_buff_len);
                *rx_len = ctx.responce_buff_len;
            }

            xSemaphoreGive(ctx.mutexSemaphore);
            return TRUE;
        }
    }

    return FALSE;
}

int cmdServerAnswerData(uint8_t *buff, uint32_t len)
{
    if (buff == NULL)
    {
        return FALSE;
    }

    if (ctx.responce_buff_len > sizeof(ctx.responce_buff))
    {
        LOG(PRINT_ERROR, "%s buffer is small", __func__, len);
        return FALSE;
    }

    memcpy(ctx.responce_buff, buff, ctx.responce_buff_len);
    xSemaphoreGive(ctx.waitResponceSem);
    return TRUE;
}

int cmdServerSetValueWithoutResp(menuValue_t val, uint32_t value)
{
    if (menuSetValue(val, value) == FALSE)
    {
        return FALSE;
    }

    static uint8_t sendBuff[8];

    sendBuff[0] = 8;
    sendBuff[1] = CMD_DATA;
    sendBuff[2] = PC_SET;
    sendBuff[3] = val;
    memcpy(&sendBuff[4], (uint8_t *)&value, 4);

    cmdServerSendData(sendBuff, 8);
    return TRUE;
}

int cmdServerSetValueWithoutRespI(menuValue_t val, uint32_t value)
{
    if (menuSetValue(val, value) == FALSE)
    {
        return FALSE;
    }

    static uint8_t sendBuff[8];

    sendBuff[0] = 8;
    sendBuff[1] = CMD_DATA;
    sendBuff[2] = PC_SET;
    sendBuff[3] = val;
    memcpy(&sendBuff[4], (uint8_t *)&value, 4);

    taskEXIT_CRITICAL(&portMux);
    cmdServerSendData(sendBuff, 8);
    taskENTER_CRITICAL(&portMux);

    return TRUE;
}

int cmdServerGetValue(menuValue_t val, uint32_t *value, uint32_t timeout)
{
    if (val >= MENU_LAST_VALUE)
    {
        return FALSE;
    }

    if (xSemaphoreTake(ctx.mutexSemaphore, timeout) == pdTRUE)
    {
        static uint8_t sendBuff[4];
        sendBuff[0] = 4;
        sendBuff[1] = CMD_REQEST;
        sendBuff[2] = PC_GET;
        sendBuff[3] = val;
        xQueueReset((QueueHandle_t)ctx.waitResponceSem);
        cmdServerSendData(sendBuff, sizeof(sendBuff));
        if (xSemaphoreTake(ctx.waitResponceSem, timeout) == pdTRUE)
        {
            if (PC_GET != ctx.responce_buff[2])
            {
                xSemaphoreGive(ctx.mutexSemaphore);
                return 0;
            }

            uint32_t return_value;

            memcpy(&return_value, ctx.responce_buff, sizeof(return_value));

            if (menuSetValue(val, *value) == FALSE)
            {
                ctx.responce_buff_len = 0;
                xSemaphoreGive(ctx.mutexSemaphore);
                return FALSE;
            }

            if (value != 0)
            {
                *value = return_value;
            }

            ctx.responce_buff_len = 0;
            xSemaphoreGive(ctx.mutexSemaphore);
            return 1;
        }
        else
        {
            xSemaphoreGive(ctx.mutexSemaphore);
            LOG(PRINT_INFO, "Timeout cmdClientGetValue");
        }
    }

    return FALSE;
}

int cmdServerSetAllValue(void)
{
    void *data;
    uint32_t data_size;
    static uint8_t sendBuff[PAYLOAD_SIZE];

    menuParamGetDataNSize(&data, &data_size);

    sendBuff[0] = 3 + data_size;
    sendBuff[1] = CMD_DATA;
    sendBuff[2] = PC_SET_ALL;
    memcpy(&sendBuff[3], data, data_size);

    cmdServerSendData(sendBuff, 3 + data_size);

    return TRUE;
}

int cmdServerGetAllValue(uint32_t timeout)
{
    if (xSemaphoreTake(ctx.mutexSemaphore, timeout) == pdTRUE)
    {
        static uint8_t sendBuff[3];

        sendBuff[0] = 3;
        sendBuff[1] = CMD_REQEST;
        sendBuff[2] = PC_GET_ALL;

        xQueueReset((QueueHandle_t)ctx.waitResponceSem);
        cmdServerSendData(sendBuff, sizeof(sendBuff));
        if (xSemaphoreTake(ctx.waitResponceSem, timeout) == pdTRUE)
        {
            if (PC_GET_ALL != ctx.responce_buff[2])
            {
                xSemaphoreGive(ctx.mutexSemaphore);
                return 0;
            }

            uint32_t return_data;
            for (int i = 0; i < (ctx.responce_buff_len - 3) / 4; i++)
            {
                return_data = (uint32_t)ctx.responce_buff[3 + i * 4];
                //LOG(PRINT_INFO, "VALUE: %d, %d", i, return_data);
                if (menuSetValue(i, return_data) == FALSE)
                {
                    LOG(PRINT_INFO, "Error Set Value %d = %d", i, return_data);
                }
            }

            ctx.responce_buff_len = 0;
            xSemaphoreGive(ctx.mutexSemaphore);
            return 1;
        }
        else
        {
            xSemaphoreGive(ctx.mutexSemaphore);
            LOG(PRINT_INFO, "Timeout cmdServerGetAllValue");
        }
    }

    return FALSE;
}

static int keepAliveSend(uint8_t *data, uint32_t dataLen)
{
    if ((ctx.state == CMD_SERVER_STATE_READY) || (ctx.state == CMD_SERVER_PARSE_RESPONSE))
    {
        if (cmdServerSendDataWaitResp(data, dataLen, NULL, NULL, 500))
        {
            return TRUE;
        }

        ctx.disconnect_req = true;
        LOG(PRINT_INFO, "KeepAliveDisco req");
    }

    return FALSE;
}

static void cmdServerErrorKACb(void)
{
    LOG(PRINT_INFO, "%s", __func__);
    ctx.disconnect_req = true;
    ctx.is_connected = false;
    menuSetValue(MENU_SERVO_IS_ON, 0);
    menuSetValue(MENU_MOTOR_IS_ON, 0);
}

void cmdServerStartTask(void)
{
    for (uint8_t i = 0; i < NUMBER_CLIENT; i++)
    {
        keepAliveInit(&ctx.keepAlive, 3000, keepAliveSend, cmdServerErrorKACb);
    }

    ctx.waitResponceSem = xSemaphoreCreateBinary();
    ctx.mutexSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(ctx.mutexSemaphore);
    cmd_server_ctx_init();
    xTaskCreate(cmd_server_task, "cmd_server_task", CONFIG_DO_TELNET_THD_WA_SIZE, NULL, NORMALPRIO, NULL);
}

void cmdServerStart(void)
{
    ctx.start = true;
}

void cmdServerStop(void)
{
    ctx.start = false;
}

bool cmdServerIsWorking(void)
{
    return ctx.is_connected;
}
