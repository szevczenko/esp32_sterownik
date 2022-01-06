#ifndef MENU_PARAM_H
#define MENU_PARAM_H

#include "config.h"
#include "parse_cmd.h"

void menuParamInit(void);
esp_err_t menuSaveParameters(void);
esp_err_t menuReadParameters(void);
void menuSetDefaultValue(void);
uint32_t menuGetValue(menuValue_t val);
uint32_t menuGetMaxValue(menuValue_t val);
uint32_t menuGetDefaultValue(menuValue_t val);
uint8_t menuSetValue(menuValue_t val, uint32_t value);
void menuParamGetDataNSize(void ** data, uint32_t * size);
void menuParamSetDataNSize(void * data, uint32_t size);

void menuPrintParameters(void);
void menuPrintParameter(menuValue_t val);

#endif