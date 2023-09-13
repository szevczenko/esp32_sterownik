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
            "DEXWAL",
            "DEXWAL"},
    [DICT_INIT] =
        {
            "Init",
            "Инициализация",
            "Inicjalizacja",
            "Initialisierung"},
    [DICT_WAIT_TO_START_WIFI] =
        {
            "Wait to start:\nWiFi",
            "Инициализация:WiFi\n",
            "Poczekaj na\nuruchomienie:\nWiFi",
            "Wait to start:\nWiFi",
        },
    [DICT_TRY_CONNECT_TO_S] =
        {
            "Try connect to:\n",
            "Соединение c:\n",
            "Łączę się do:\n",
            "Ich verbinde\nmich mit:\n"},
    [DICT_WAIT_CONNECTION_S_S_S] =
        {
            "         Wait connection%s%s%s",
            "               Ожидание \n             соединения%s%s%s",
            "              Oczekuję na \n                połączenie%s%s%s",
            "     Verbindung warten%s%s%s"},
    [DICT_CONNECTED_TRY_READ_DATA] =
        {
            "Connected\n Try read data",
            "Подключено\n Попытка чтения данных",
            "Połączono\n Próbuję odczytać dane",
            "Verbunden, Data lesen"},
    [DICT_READ_DATA_FROM_S] =
        {
            "Reading data from:\n%s",
            "Чтение данных из:\n%s",
            "Odczytywanie danych z:\n%s",
            "Data lesen:\n%s"},
    [DICT_SYSTEM_READY_TO_START] =
        {
            "System ready to start",
            "Система готова к запуску",
            "    System gotowy\n      do uruchomienia",
            "System ist startbereit"},
    /* MENU */
    [DICT_SETTINGS] =
        {
            "Settings",
            "Настройки",
            "USTAWIENIA",
            "Einstellungen"},
    [DICT_START] =
        {
            "Start",
            "старт",
            "START",
            "Start"},
    [DICT_DEVICES] =
        {
            "Devices",
            "устройства",
            "URZĄDZENIA",
            "Geräte"},
    [DICT_PARAMETES] =
        {
            "Parameters",
            "Параметры",
            "PARAMETRY",
            "Parameter"},
    [DICT_LOW_BAT] =
        {
            "Low battery",
            "низкий заряд батареи",
            "Niski poziom baterii",
            "Niedriger\nBatteriestatus"},
    [DICT_MENU] =
        {
            "Menu",
            "Меню",
            "MENU",
            "Menü"},
    /* MENU DRV */
    [DICT_WAIT_TO_INIT] =
        {
            "Wait to init",
            "Подождите, \n чтобы инициализировать",
            "Czekaj",
            "Warten Sie"},
    [DICT_MENU_IDLE_STATE] =
        {
            "IDLE STATE",
            "ПРОСТОЕ СОСТОЯНИЕ",
            "Stan bezczynności",
            "LEERLAUF"},
    [DICT_MENU_STOP] =
        {
            "                STOP",
            "                CTOП",
            "                STOP",
            "                STOP"},
    [DICT_POWER_OFF] =
        {
            "  Power off",
            "  Выключение",
            "  Wyłączanie",
            "Ausschalten"},
    /* SETTINGS */
    [DICT_BOOTING] =
        {
            "Booting",
            "Загрузка",
            "Uruchamianie",
            "Booten"},
    [DICT_BUZZER] =
        {
            "Sound",
            "Звук",
            "Dźwięk",
            "Klang"},
    [DICT_LANGUAGE] =
        {
            "Language",
            "Язык",
            "Język",
            "Sprache"},
    [DICT_IDLE_TIME] =
        {
            "Idle time",
            "Время простоя",
            "Wyłącz po",
            "Wartezeit"},
    [DICT_MOTOR_ERR] =
        {
            "Motor error",
            "Oшибка двигателя",
            "Błąd silnika",
            "Motorfehler"},
    [DICT_SERVO_ERR] =
        {
            "Servo error",
            "Ошибка сервопривода",
            "Błąd serwo",
            "Servofehler"},
    [DICT_VIBRO_ERR] =
        {
            "Vibro error",
            "Oшибка вибратора",
            "Błąd wibro",
            "Vibratorfehler"},
    [DICT_PERIOD] =
        {
            "Period",
            "Период",
            "Okres",
            "Zeitspanne"},
    [DICT_BRIGHTNESS] =
        {
            "Brightness",
            "Яркость",
            "Jasność",
            "Helligkeit"},
    [DICT_MOTOR_ERROR_CALIBRATION] =
        {
            "Motor err calib",
            "Рег. ошибка двигателя",
            "Reg. błędu silnika",
            "Motorkalibrierung"},
    [DICT_SERVO_CLOSE] =
        {
            "Servo close",
            "Сервопривод закрыть",
            "Serwo zamknięty",
            "Servo schließen"},
    [DICT_SERVO_OPEN] =
        {
            "Servo open",
            "открытый сервопривод",
            "Serwo otwarty",
            "Servo geöffnet"},
    [DICT_SILOS_HEIGHT] =
        {
            "Silos height",
            "Высота",
            "Wysokość zbiornika",
            "Silos height"},
    [DICT_ON] =
        {
            "ON",
            "Активировано",
            "Włączony",
            "Auf"},
    [DICT_OFF] =
        {
            "OFF",
            "Выключенный",
            "Wyłączony",
            "Aus"},
    /* PARAMETERS */
    [DICT_CURRENT] =
        {
            "Current",
            "Текущий",
            "Prąd",
            "Strom"},
    [DICT_VOLTAGE] =
        {
            "Voltage",
            "вольтаж",
            "Napięcie",
            "Spannung"},
    [DICT_SILOS] =
        {
            "Silos",
            "Силосы",
            "Zbiornik",
            "Silos",
        },
    [DICT_SIGNAL] =
        {
            "Signal",
            "Сигнал",
            "Sygnał",
            "Signal"},
    [DICT_TEMP] =
        {
            "Temp",
            "темп",
            "Temp",
            "Temp"},
    [DICT_CONNECT] =
        {
            "Connect",
            "соединять",
            "Połączenie",
            "Verbindung"},
    [DICT_DEVICE_NOT_CONNECTED] =
        {
            "\n               Device not \n                connected",
            "\n           устройство не \n             подключено",
            "\n                Urządzenie \n              niepołączone",
            "\n                Gerät nicht\n                verbunden"},
    /* WIFI */
    [DICT_WAIT_TO_WIFI_INIT] =
        {
            "Wait to wifi init",
            "Дождитесь инициализации wifi",
            "Poczekaj na\nuruchomienie Wi-Fi",
            "Warten Sie auf\nden Start des Wi-Fi"},
    [DICT_SCANNING_DEVICES] =
        {
            "scanning devices",
            "сканирующие устройства",
            "Wyszukiwanie urządzeń",
            "Gerät suchen"},
    [DICT_CLICK_ENTER_TO_SCANNING] =
        {
            "Click enter to scanning",
            "Нажмите Enter для сканирования",
            "Kliknij Enter, aby wyszukać",
            "Klicken Sie zum\nScannen auf Enter"},
    [DICT_FIND_DEVICES] =
        {
            "\n              Find Devices",
            "\n      Найти устройства",
            "\n         Szukam urządzeń",
            "\n              Geräte finden"},
    [DICT_DEVICE_NOT_FOUND] =
        {
            "\n           Device not found",
            "\n Устройство не найдено",
            "\n             Urządzenie nie\n         zostało znalezione",
            "\n                 Gerät nicht\n                   gefunden"},
    [DICT_TRY_CONNECT_TO] =
        {
            "Try connect to",
            "Попробуйте подключиться к",
            "Próba połączenia z\n",
            "Versuchen Sie eine\nVerbindung zu"},
    [DICT_WAIT_TO_CONNECT] =
        {
            "Wait to connect",
            "Подождите, чтобы подключиться",
            "Poczekaj na połączenie",
            "Warten auf Verbindung"},
    [DICT_WIFI_CONNECTED] =
        {
            "WIFI CONNECTED",
            "Подключено к устройству",
            "Połączono z urządzeniem",
            "Verbunden mit dem Gerät"},
    [DICT_WAIT_TO_SERVER] =
        {
            "Wait to server",
            "Подождите подключения",
            "Poczekaj na połączenie",
            "Warten auf Server"},
    [DICT_ERROR_CONNECT] =
        {
            "\n\n     ERROR CONNECT",
            "Ошибка подключения",
            "Bląd połączenia",
            "       FEHLERHAFTE\n       VERBINDUNG"},
    [DICT_CONNECTED_TO] =
        {
            "Connected to",
            "ошибка подключения",
            "Błąd połączenia",
            "Verbunden mit"},
    /* LOW BATTERY */
    [DICT_LOW_BATTERY] =
        {
            "      LOW BATTERY",
            "З АРЯД БАТАРЕИ",
            "NISKI POZIOM BATERII",
            "   BATTERIE TIEF"},
    [DICT_CONNECT_CHARGER] =
        {
            "           Connect charger",
            " Подключить \n зарядное устройство",
            "   Podłącz ładowarkę",
            "   Ladegerät verbinden"},
    [DICT_CHECK_CONNECTION] =
        {
            "             Check connection",
            " Проверка соединения",
            " Sprawdzanie połączenia",
            "        Verbindung prüfen"},
    [DICT_VIBRO_ON] =
        {
            "Vibro",
            "вибро",
            "Wibro",
            "Vibrator"
        },
    [DICT_VIBRO_OFF] =
        {
            "Vibro off",
            "вибро вык",
            "Wibro wył",
            "Vibrator aus"},
    [DICT_LOW] =
        {
            "           Empty    \n                  Silos",
            "         Пустой   \n           Силосы   ",
            "              Pusty     \n            zbiornik \n               ",
            "        Niedrig \n                  Silos"},
    [DICT_MOTOR] =
        {
            "Motor",
            "Мотор",
            "Silnik",
            "Motor"},
    [DICT_SERVO] =
        {
            "Servo",
            "Серво",
            "Serwo",
            "Servo"},
    [DICT_TARGET_NOT_CONNECTED] =
        {
            "target not connected",
            "Ошибка подключения",
            "Połączenie nie udane",
            "Gerät nicht verbunden"},
    [DICT_POWER_SAVE] =
        {
            "Power save",
            "Энергосбережение",
            "Oszczędzanie energii",
            "Energie sparen"},
    [DICT_SERVO_NOT_CONNECTED] =
        {
            "Servo not\n connected",
            "Серво не подключен",
            "Serwo nie \npodłączone"
            "Servo nicht verbunden"},
    [DICT_SERVO_OVERCURRENT] =
        {
            "                  Servo \n            overcurrent",
            "               Серво \n       заблокирован",
            "                   Servo \n         Zablokowane",
            "                    Servo \n                gesperrt"},
    [DICT_MOTOR_NOT_CONNECTED] =
        {
            "Motor not \nconnected",
            "он не \nопределяет двигатель",
            "          Nie wykryto\n                    silnika",
            "          Motor nicht\n          verbunden"},
    [DICT_VIBRO_NOT_CONNECTED] =
        {
            "Vibro not\nconnected",
            "Двигатель вибратора\nне подключен",
            "  Silnik wibratora     nie podłączony",
            "          Vibro nicht\n          verbunden"},
    [DICT_VIBRO_OVERCURRENT] =
        {
            "Vibro overcurrent",
            "Мотор вибратора\nзаблокирован",
            "  Silnik wibratora     Zablokowany",
            "Vibro gesperrt"},
    [DICT_MOTOR_OVERCURRENT] =
        {
            "                  Motor\n           overcurrent",
            "          Мотор\nзаблокированны",
            "                   Silnik\n         Zablokowany",
            "             Motor \n         gesperrt"},
    [DICT_TEMPERATURE_IS_HIGH] =
        {
            "Temperature \nis high",
            "Температура \nвысокая",
            "Zbyt wysoka \ntemperatura",
            "Zu hohe Temperatur"},
    [DICT_UNKNOWN_ERROR] =
        {
            "Unknown error",
            "неизвестная ошибка",
            "Nieznany błąd",
            "Unbekannter Fehler"},
    [DICT_LOST_CONNECTION_WITH_SERVER] =
        {
            "       Lost connection \n          with server",
            "потеря связи с сервером",
            " \n   Utacono połączenie \n         z serwerem",
            "Verbindung zum\nServer unterbrochen"},
    [DICT_TIMEOUT_CONNECT] =
        {
            "Timeout connect",
            "Время ожидания \n подключения",
            "Przekroczono limit \n czasu połączenia",
            "    Zeitüberschreitung\n        verbinden"},
    [DICT_TIMEOUT_SERVER] =
        {
            "Timeout server",
            "Сервер тайм-аута",
            "Przekroczono czas \n odpowiedzi serwera",
            "  Zeitüberschreitung\n         Server"},
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

const char *dictionary_get_string(enum dictionary_phrase phrase)
{
    if (phrase < DICT_TOP)
    {
        return dictionary_phrases[phrase][dict_language];
    }

    return "Bad phrase number";
}