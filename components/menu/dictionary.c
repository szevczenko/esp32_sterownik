#include "dictionary.h"
#include "config.h"
#include "menu_param.h"

static menu_language_t dict_language = MENU_LANGUAGE_ENGLISH;

static const char *dictionary_phrases[DICT_TOP][LANGUAGE_CNT_SUPPORT] = {
    /* Bootup */
    [DICT_LOGO_CLIENT_NAME] = 
    {
        "DEXWAL",
        "DEXWAL",
        "DEXWAL"
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
        "Инициализация:WiFi\n",
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
        "Settings",
        "Настройки",
        "Ustawienia"
    },
    [DICT_START] =
    { 
        "Start",
        "старт",
        "Start"
    },
    [DICT_DEVICES] =
    { 
        "Devices",
        "устройства",
        "Urządzenia"
    },
    [DICT_PARAMETES] =
    { 
        "Parameters",
        "Параметры",
        "Parametry"
    },
    [DICT_LOW_BAT] =
    { 
        "Low battery",
        "низкий заряд батареи",
        "Niski poziom baterii"
    },
    [DICT_MENU] =
    { 
        "Menu",
        "Меню",
        "Menu"
    },
    /* MENU DRV */
    [DICT_WAIT_TO_INIT] =
    { 
        "Wait to init",
        "Подождите, \n чтобы инициализировать",
        "Czekaj"
    },
    [DICT_MENU_IDLE_STATE] =
    { 
        "IDLE STATE",
        "ПРОСТОЕ СОСТОЯНИЕ",
        "Stan bezczynności"
    },
    [DICT_MENU_STOP] =
    { 
        "STOP",
        "Oстановка",
        "STOP"
    },
    [DICT_POWER_OFF] =
    { 
        "Power off...",
        "Выключение...",
        "Wyłączanie..."
    },
    /* SETTINGS */
    [DICT_BOOTING] =
    { 
        "Booting",
        "Загрузка",
        "Uruchamianie"
    },
    [DICT_BUZZER] =
    { 
        "Sound",
        "Звук",
        "Dźwięk"
    },
    [DICT_LANGUAGE] =
    { 
        "Language",
        "Язык",
        "Język"
    },
    [DICT_IDLE_TIME] =
    { 
        "Idle time",
        "Время простоя",
        "Czas bezczynności"
    },
    [DICT_MOTOR_ERR] =
    { 
        "Motor error",
        "Oшибка двигателя",
        "Błąd silnika"
    },
    [DICT_SERVO_ERR] =
    { 
        "Servo error",
        "Ошибка сервопривода",
        "Błąd serwomechanizmu"
    },
    [DICT_VIBRO_ERR] =
    { 
        "Vibro error",
        "Oшибка вибратора",
        "Błąd wibratora"
    },
    [DICT_SERVO_CLOSE] =
    { 
        "Servo close",
        "Сервопривод закрыть",
        "Serwomechanizm zamknięty"
    },
    [DICT_SERVO_OPEN] =
    { 
        "Servo open",
        "открытый сервопривод",
        "Serwomechanizm otwarty"
    },
    [DICT_ON] =
    { 
        "ON",
        "Активировано",
        "Włączony"
    },
    [DICT_OFF] =
    { 
        "OFF",
        "Выключенный",
        "Wyłączony"
    },
    /* PARAMETERS */
    [DICT_CURRENT] =
    { 
        "Current",
        "Текущий",
        "Prąd"
    },
    [DICT_VOLTAGE] =
    { 
        "Voltage",
        "вольтаж",
        "Napięcie"
    },
    [DICT_SILOS] =
    { 
        "Silos",
        "Силосы",
        "Zbiornik"
    },
    [DICT_SIGNAL] =
    { 
        "Signal",
        "Сигнал",
        "Sygnał"
    },
    [DICT_TEMP] =
    { 
        "Temp",
        "темп",
        "Temp"
    },
    [DICT_CONNECT] =
    { 
        "Connect",
        "соединять",
        "Połączenie"
    },
    [DICT_DEVICE_NOT_CONNECTED] =
    { 
        "Device not connected",
        "устройство не подключено",
        "Urządzenie niepołączone"
    },
    /* WIFI */
    [DICT_WAIT_TO_WIFI_INIT] =
    { 
        "Wait to wifi init",
        "Дождитесь инициализации wifi",
        "Poczekaj na uruchomienie Wi-Fi"
    },
    [DICT_SCANNING_DEVICES] =
    { 
        "scanning devices",
        "сканирующие устройства",
        "Wyszukiwanie urządzeń"
    },
    [DICT_CLICK_ENTER_TO_SCANNING] =
    { 
        "Click enter to scanning",
        "Нажмите Enter для сканирования",
        "Kliknij Enter, aby wyszukać"
    },
    [DICT_FIND_DEVICES] =
    { 
        "Find Devices",
        "Найти устройства",
        "Znajdź urządzenia"
    },
    [DICT_DEVICE_NOT_FOUND] =
    { 
        "Device not found",
        "Устройство не найдено",
        "Urządzenie nie zostało znalezione"
    },
    [DICT_TRY_CONNECT_TO] =
    { 
        "Try connect to",
        "Попробуйте подключиться к",
        "Próba połączenia z\n"
    },
    [DICT_WAIT_TO_CONNECT] =
    { 
        "Wait to connect",
        "Подождите, чтобы подключиться",
        "Poczekaj na połączenie"
    },
    [DICT_WIFI_CONNECTED] =
    { 
        "WIFI CONNECTED",
        "Подключено к устройству",
        "Połączono z urządzeniem"
    },
    [DICT_WAIT_TO_SERVER] =
    { 
        "Wait to server",
        "Подождите подключения",
        "Poczekaj na połączenie"
    },
    [DICT_ERROR_CONNECT] =
    { 
        "ERROR CONNECT",
        "Ошибка подключения",
        "Bląd połączenia"
    },
    [DICT_CONNECTED_TO] =
    { 
        "Connected to",
        "ошибка подключения",
        "Błąd połączenia"
    },
    /* LOW BATTERY */
    [DICT_LOW_BATTERY] =
    { 
        "LOW BATTERY",
        "ЗАРЯД БАТАРЕИ",
        "NISKI POZIOM BATERII"
    },
    [DICT_CONNECT_CHARGER] =
    { 
        "Connect charger",
        " Подключить \n зарядное устройство",
        "Podłącz ładowarkę"
    },
    [DICT_CHECK_CONNECTION] =
    { 
        "Check connection",
        "Проверка соединения",
        "Sprawdzanie połączenia"
    },
    [DICT_VIBRO_ON] =
    { 
        "Vibro on",
        "вибро на",
        "Wibro Wł"
    },
    [DICT_VIBRO_OFF] =
    { 
        "Vibro off",
        "вибро вык",
        "Wibro wył"
    },
    [DICT_LOW] =
    { 
        "Low",
        "Низкий",
        "Niski"
    },
    [DICT_MOTOR] =
    { 
        "Motor",
        "Мотор",
        "Silnik"
    },
    [DICT_SERVO] =
    { 
        "servo",
        "Серво",
        "Serwo"
    },
    [DICT_TARGET_NOT_CONNECTED] =
    { 
        "target not connected",
        "Ошибка подключения",
        "Połączenie nie udane"
    },
    [DICT_POWER_SAVE] =
    { 
        "Power save",
        "Энергосбережение",
        "Oszczędzanie energii"
    },
    [DICT_SERVO_NOT_CONNECTED] =
    { 
        "Servo not connected",
        "Серво не подключен",
        "Serwo nie podłączone"
    },
    [DICT_SERVO_OVERCURRENT] =
    { 
        "Servo overcurrent",
        "Серво заблокирован",
        "Servo zablokowane"
    },
    [DICT_MOTOR_NOT_CONNECTED] =
    { 
        "Motor not connected",
        "он не определяет двигатель",
        "Nie wykryto silnika"
    },
    [DICT_VIBRO_NOT_CONNECTED] =
    { 
        "Vibro not connected",
        "Двигатель вибратора не подключен",
        "Silnik wibratora nie podłączony"
    },
    [DICT_VIBRO_OVERCURRENT] =
    { 
        "Vibro overcurrent",
        "Мотор вибратора заблокирован",
        "Silnik wibratora zablokowany"
    },
    [DICT_MOTOR_OVERCURRENT] =
    { 
        "Motor overcurrent",
        "Мотор заблокированный",
        "Silnik zablokowany"
    },
    [DICT_TEMPERATURE_IS_HIGH] =
    { 
        "Temperature is high",
        "Температура высокая",
        "Zbyt wysoka temperatura"
    },
    [DICT_UNKNOWN_ERROR] =
    { 
        "Unknown error",
        "неизвестная ошибка",
        "Nieznany błąd"
    },
    [DICT_LOST_CONNECTION_WITH_SERVER] =
    { 
        "Lost connection with server",
        "потеря связи с сервером",
        "Utacono połączenie \n z serwerem"
    },
    [DICT_TIMEOUT_CONNECT] =
    { 
        "Timeout connect",
        "Время ожидания \n подключения",
        "Przekroczono limit \n czasu połączenia"
    },
    [DICT_TIMEOUT_SERVER] =
    { 
        "Timeout server",
        "Сервер тайм-аута",
        "Przekroczono czas \n odpowiedzi serwera"
    },
};

void dictionary_init(void)
{
    dictionary_set_language(menuGetValue(MENU_LANGUAGE));
}

bool dictionary_set_language(menu_language_t lang)
{
    if (lang < LANGUAGE_CNT_SUPPORT)
    {
        menuSetValue(MENU_LANGUAGE, lang);
        dict_language = lang;
        return true;
    }

    return false;
}

const char * dictionary_get_string(enum dictionary_phrase phrase)
{
    if (phrase < DICT_TOP)
    {
        return dictionary_phrases[phrase][dict_language];
    }

    return "Bad phrase number";
}