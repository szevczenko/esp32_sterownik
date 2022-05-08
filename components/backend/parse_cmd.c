#include "parse_cmd.h"
#include "cmd_client.h"
#include "cmd_server.h"
#include "keepalive.h"

#include "menu_param.h"
#include "error_siewnik.h"

#define MODULE_NAME    "[PARSE] "
#define DEBUG_LVL      PRINT_INFO

#if CONFIG_DEBUG_PARSE_CMD
#define LOG(_lvl, ...)                          \
    debug_printf(DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__)
#else
#define LOG(PRINT_INFO, ...)
#endif

#define PAYLOAD_SIZE 256

static uint8_t sendBuff[PAYLOAD_SIZE];
static uint32_t frameLenClient;

void parse_client_buffer(uint8_t *buff, uint32_t len)
{
    uint32_t parsed_len = 0;

    do
    {
        frameLenClient = buff[parsed_len];

        if (frameLenClient > len)
        {
            LOG(PRINT_ERROR, "Bad lenth", frameLenClient, len, parsed_len, buff);
            break;
        }

        if (frameLenClient == 0)
        {
            LOG(PRINT_ERROR, "Lenth is 0", frameLenClient, len, parsed_len, buff);
            break;
        }

        LOG(PRINT_DEBUG, "frameLenClient %d len %d parsed_len %d %p", frameLenClient, len, parsed_len, buff);

        parse_client(&buff[parsed_len], frameLenClient);
        len -= frameLenClient;
        parsed_len += frameLenClient;
    } while (len > 0);
}

void parse_client(uint8_t *buff, uint32_t len)
{
    uint32_t value;

    LOG(PRINT_DEBUG, "Parse: %d %d", buff[1], buff[2]);
    if ((buff[1] == CMD_REQEST) || (buff[1] == CMD_DATA))
    {
        switch (buff[2])
        {
        case PC_KEEP_ALIVE:
            sendBuff[0] = 3;
            sendBuff[1] = CMD_ANSWER;
            sendBuff[2] = PC_KEEP_ALIVE;
            if (buff[1] != CMD_DATA)
            {
                cmdClientSend(sendBuff, 3);
            }

            break;

        case PC_GET:
            sendBuff[0] = 8;
            sendBuff[1] = CMD_ANSWER;
            sendBuff[2] = PC_GET;
            sendBuff[3] = POSITIVE_RESP;
            value = menuGetValue(buff[3]);
            memcpy(&sendBuff[4], &value, 4);
            cmdClientSend(sendBuff, 8);
            break;

        case PC_SET:
            sendBuff[0] = 4;
            sendBuff[1] = CMD_ANSWER;
            sendBuff[2] = PC_SET;
            memcpy(&value, &buff[4], 4);
            if (menuSetValue(buff[3], value))
            {
                menuPrintParameter(buff[3]);
                sendBuff[3] = POSITIVE_RESP;
            }
            else
            {
                sendBuff[3] = NEGATIVE_RESP;
            }

            if (buff[1] != CMD_DATA)
            {
                cmdClientSend(sendBuff, 4);
            }

            break;

        case PC_SET_ALL:
           {
               sendBuff[0] = 4;
               sendBuff[1] = CMD_ANSWER;
               sendBuff[2] = PC_SET_ALL;

               uint32_t *return_data = (uint32_t *)&buff[3];
               for (int i = 0; i < (len - 3) / 4; i++)
               {
                   if (menuSetValue(i, return_data[i]) == FALSE)
                   {
                       LOG(PRINT_INFO, "Parse Error Set Value %d = %d", i, return_data[i]);
                   }
                   else
                   {
                       LOG(PRINT_INFO, "Parse Set Value %d = %d", i, return_data[i]);
                   }
               }

               sendBuff[3] = POSITIVE_RESP;

               if (buff[1] != CMD_DATA)
               {
                   cmdClientSend(sendBuff, 4);
               }

               //menuPrintParameters();
           }
           break;

        case PC_GET_ALL:
           {
               void *data;
               uint32_t data_size;

               menuParamGetDataNSize(&data, &data_size);

               if (sizeof(sendBuff) < 3 + data_size)
               {
                   LOG(PRINT_ERROR, "Buffer is to small");
                   break;
               }

               sendBuff[0] = 3 + data_size;
               sendBuff[1] = CMD_ANSWER;
               sendBuff[2] = PC_GET_ALL;

               memcpy(&sendBuff[3], data, data_size);
               cmdClientSend(sendBuff, 3 + data_size);
           }
           break;
        }
    }
    else if (buff[1] == CMD_ANSWER)
    {
        //LOG(PRINT_INFO, "Answer data %d", len);
        switch (buff[2])
        {
        case PC_SET:
        case PC_GET:
        case PC_GET_ALL:
        case PC_SET_ALL:
        case PC_KEEP_ALIVE:
            cmdClientAnswerData(buff, len);
            break;
        }
    }
    else if (buff[1] == CMD_COMMAND)
    {
        switch (buff[2])
        {
        case PC_CMD_RESET_ERROR:

            break;
        }
    }
}

static uint32_t frameLenServer;

void parse_server_buffer(uint8_t *buff, uint32_t len)
{
    uint32_t parsed_len = 0;

    do
    {
        frameLenServer = buff[parsed_len];

        if (frameLenServer > len)
        {
            break;
        }

        parse_server(&buff[parsed_len], frameLenServer);
        len -= frameLenServer;
        parsed_len += frameLenServer;
    } while (len > 0);
}

void parse_server(uint8_t *buff, uint32_t len)
{
    //uint32_t * val;
    debug_function_name("parse_server");
    //LOG(PRINT_INFO, "Parse: %d %d", buff[1], buff[2]);
    uint32_t value;

    if ((buff[1] == CMD_REQEST) || (buff[1] == CMD_DATA))
    {
        switch (buff[2])
        {
        case PC_KEEP_ALIVE:
            sendBuff[0] = 3;
            sendBuff[1] = CMD_ANSWER;
            sendBuff[2] = PC_KEEP_ALIVE;
            if (buff[1] != CMD_DATA)
            {
                cmdServerSendData(sendBuff, 3);
            }

            break;

        case PC_GET:
            sendBuff[0] = 8;
            sendBuff[1] = CMD_ANSWER;
            sendBuff[2] = PC_GET;
            sendBuff[3] = POSITIVE_RESP;
            value = menuGetValue(buff[3]);
            memcpy(&sendBuff[4], &value, 4);
            if (buff[1] != CMD_DATA)
            {
                cmdServerSendData(sendBuff, 8);
            }

            break;

        case PC_SET:
            sendBuff[0] = 4;
            sendBuff[1] = CMD_ANSWER;
            sendBuff[2] = PC_SET;
            memcpy(&value, &buff[4], 4);
            if (menuSetValue(buff[3], value))
            {
                sendBuff[3] = POSITIVE_RESP;
            }
            else
            {
                sendBuff[3] = NEGATIVE_RESP;
            }

            if (buff[1] != CMD_DATA)
            {
                cmdServerSendData(sendBuff, 4);
            }

            menuPrintParameter(buff[3]);
            break;

        case PC_SET_ALL:
           {
               sendBuff[0] = 4;
               sendBuff[1] = CMD_ANSWER;
               sendBuff[2] = PC_SET_ALL;

               uint32_t return_data;
               for (int i = 0; i < (len - 3) / 4; i++)
               {
                   return_data = (uint32_t)buff[3 + i * 4];
                   //LOG(PRINT_INFO, "VALUE: %d, %d", i, return_data);
                   if (menuSetValue(i, return_data) == FALSE)
                   {
                       LOG(PRINT_INFO, "Parse Error Set Value %d = %d", i, return_data);
                   }
               }

               sendBuff[3] = POSITIVE_RESP;

               if (buff[1] != CMD_DATA)
               {
                   cmdServerSendData(sendBuff, 4);
               }

               //menuPrintParameters();
           }
           break;

        case PC_GET_ALL:
           {
               void *data;
               uint32_t data_size;

               menuParamGetDataNSize(&data, &data_size);
               LOG(PRINT_DEBUG, "GET_ALL data %p, size %d", data, data_size);

               if (sizeof(sendBuff) < 3 + data_size)
               {
                   LOG(PRINT_ERROR, "Buffer is to small");
                   break;
               }

               sendBuff[0] = 3 + data_size;
               sendBuff[1] = CMD_ANSWER;
               sendBuff[2] = PC_GET_ALL;

               memcpy(&sendBuff[3], data, data_size);
               cmdServerSendData(sendBuff, 3 + data_size);
           }
           break;
        }
    }
    else if (buff[1] == CMD_ANSWER)
    {
        switch (buff[2])
        {
        LOG(PRINT_DEBUG, "Answer data %d", len);
        case PC_KEEP_ALIVE:
        case PC_SET:
        case PC_GET:
        case PC_GET_ALL:
        case PC_SET_ALL:
            cmdServerAnswerData(buff, len);
            break;
        }
    }
    else if (buff[1] == CMD_COMMAND)
    {
        switch (buff[2])
        {
        case PC_CMD_RESET_ERROR:
#if CONFIG_DEVICE_SIEWNIK
            //errorReset();
            LOG(PRINT_INFO, "error reset");
#endif
            break;
        }
    }
}
