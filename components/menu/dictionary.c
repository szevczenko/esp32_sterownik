#include "dictionary.h"
#include "config.h"
#include "menu_param.h"

static const char **dictionary_phrases[DICT_TOP] = {
    /* Bootup */
    [DICT_LOGO_CLIENT_NAME] = 
    {
        "DEXVAL",
        "DEXVAL",
        "DEXVAL"
    },
    [DICT_INIT] = 
    {
        "Init",
        "Инициализация",
        "Inicjalizacja"
    },
    [DICT_WAIT_TO_START_WIFI] = 
    {
        "Wait to start:\nWiFi",
        "Инициализация:\nWiFi",
        "Poczekaj na\nuruchomienie:\nWiFi"
    },
    [DICT_TRY_CONNECT_TO_S] = 
    {
        "Try connect to:\n",
        "Соединение c:\n"
        "Łączę się do:\n"
    },
    [DICT_WAIT_CONNECTION_S_S_S] = 
    {
        "Wait connection%s%s%s",
        "Ожидание соединения%s%s%s",
        "Oczekuję na połączenie%s%s%s"
    },
    [DICT_CONNECTED_TRY_READ_DATA] = 
    {
        "Connected:\n%s\n Try read data",
        "Подключено:\n%s\n Попытка чтения данных",
        "Połączono:\n%s\n Próbuję odczytać dane"
    },
    [DICT_READ_DATA_FROM_S] = 
    {
        "Reading data from:\n%s",
        "Чтение данных из:\n%s",
        "Odczytywanie danych z:\n%s"
    },
    [DICT_SYSTEM_READY_TO_START] = 
    { 
        "System ready to start",
        "Система готова к запуску",
        "System gotowy do uruchomienia"
    },
    /* MENU */
    [DICT_SETTINGS] =
    { 
        "System ready to start",
        "Система готова к запуску",
        "System gotowy do uruchomienia"
    },
    [DICT_START] =
    { 
        "System ready to start",
        "Система готова к запуску",
        "System gotowy do uruchomienia"
    },
    [DICT_DEVICES] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_PARAMETES] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_LOW_BAT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_MENU] =
    { 
        "---",
        "---",
        "---"
    },
    /* MENU DRV */
    [DICT_WAIT_TO_INIT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_MENU_IDLE_STATE] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_MENU_STOP] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_POWER_OFF] =
    { 
        "---",
        "---",
        "---"
    },
    /* SETTINGS */
    [DICT_BOOTING] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_BUZZER] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_LANGUAGE] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_IDLE_TIME] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_MOTOR_ERR] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_SERVO_ERR] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_VIBRO_ERR] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_SERVO_CLOSE] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_SERVO_OPEN] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_ON] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_OFF] =
    { 
        "---",
        "---",
        "---"
    },
    /* PARAMETERS */
    [DICT_CURRENT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_VOLTAGE] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_SILOS] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_SIGNAL] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_TEMP] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_CONNECT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_DEVICE_NOT_CONNECTED] =
    { 
        "---",
        "---",
        "---"
    },
    /* WIFI */
    [DICT_WAIT_TO_WIFI_INIT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_SCANNING_DEVICES] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_CLICK_ENETER_TO_SCANNING] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_FIND_DEVICES] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_DEVICE_NOT_FOUND] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_TRY_CONNECT_TO] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_WAIT_TO_CONNECT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_WIFI_CONNECTED] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_WAIT_TO_SERVER] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_ERROR_CONNECT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_CONNECTED_TO] =
    { 
        "---",
        "---",
        "---"
    },
    /* LOW BATTERY */
    [DICT_LOW_BATTERY] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_CONNECT_CHARGER] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_CHECK_CONNECTION] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_VIBRO_ON] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_VIBRO_OFF] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_LOW] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_MOTOR] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_SERVO] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_TARGET_NOT_CONNECTED] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_POWER_SAVE] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_SERVO_NOT_CONNECTED] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_SERVO_OVERCURRENT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_MOTOR_NOT_CONNECTED] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_VIBRO_NOT_CONNECTED] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_VIBRO_OVERCURRENT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_MOTOR_OVERCURRENT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_TEMPERATURE_IS_HIGH] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_UNKNOWN_ERROR] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_LOST_CONNECTION_WITH_SERVER] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_TIMEOUT_CONNECT] =
    { 
        "---",
        "---",
        "---"
    },
    [DICT_TIMEOUT_SERVER] =
    { 
        "---",
        "---",
        "---"
    },
}