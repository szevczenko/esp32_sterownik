#ifndef OLED_H_
#define OLED_H_

#include "ssd1306.h"
#include "oled_glcd.h"
#include "ssd1306_fonts.h"

extern const PROGMEM uint8_t Calibri16x17 [];
extern const PROGMEM uint8_t Small_Fonts8x9[];
extern const PROGMEM uint8_t Calibri11x11[];
extern const PROGMEM uint8_t Calibri_RU_11x11[];
extern const PROGMEM uint8_t Calibri10x11_PL[];

enum oledFontSize
{
    OLED_FONT_SIZE_11,
    OLED_FONT_SIZE_16,
    OLED_FONT_SIZE_LAST
};

void oled_init(void);
void oled_clearScreen(void);
void oled_printFixed(lcdint_t xpos, lcdint_t y, const char *ch, enum oledFontSize font_size);
void oled_printFixedBlack(lcdint_t xpos, lcdint_t y, const char *ch, enum oledFontSize font_size);
void oled_update(void);
void oled_setCursor(lcdint_t xpos, lcdint_t y);
void oled_print(const char *ch);
void oled_printBlack(const char *ch);
void oled_putPixel(lcdint_t x, lcdint_t y);
void oled_clearPixel(lcdint_t x, lcdint_t y);

void oled_getGLCDCharBitmap(uint16_t unicode, SCharInfo *info);
void oled_setGLCDFont(enum oledFontSize font_size);
uint8_t oled_printGLCDChar(uint8_t c);
uint8_t draw_GLCD(lcdint_t x, lcdint_t y, lcduint_t w, lcduint_t h, const uint8_t *bitmap);

#endif