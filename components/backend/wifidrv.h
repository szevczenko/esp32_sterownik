#ifndef _WIFI_DRV_H
#define _WIFI_DRV_H

#include "config.h"

#define WIFI_SOLARKA_NAME "SOLA"
#define WIFI_SIEWNIK_NAME "SIEW"

#if CONFIG_DEVICE_SIEWNIK
#define WIFI_AP_NAME WIFI_SIEWNIK_NAME
#endif

#if CONFIG_DEVICE_SOLARKA
#define WIFI_AP_NAME WIFI_SOLARKA_NAME
#endif

#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD "SuperTrudne1!-_"
#endif

typedef struct
{
  char ssid[33];
  char password[64];
}wifiConData_t;

void wifiDrvInit(void);
int wifiDataSave(wifiConData_t *data);
esp_err_t wifiDataRead(wifiConData_t *data);

int wifiDrvSetFromAPList(uint8_t num);
int wifiDrvSetAPName(char* name, size_t len);
int wifiDrvSetPassword(char* passwd, size_t len);
int wifiDrvConnect(void);
int wifiDrvDisconnect(void);
int wifiStartAccessPoint(void);
int wifiStartDevice(void);
int wifiDrvIsConnected(void);
bool wifiDrvIsReadyToScan(void);
bool wifiDrvReadyToConnect(void);
bool wifiDrvTryingConnect(void);

int wifiDrvStartScan(void);
int wifiDrvGetAPName(char* name);
int wifiDrvGetNameFromScannedList(uint8_t number, char * name);
void wifiDrvGetScanResult(uint16_t *ap_count);
int wifiDrvGetRssi(void);
bool wifiDrvIsReadedData(void);
bool wifiDrvIsIdle(void);
void wifiDrvPowerSave(bool state);

#endif