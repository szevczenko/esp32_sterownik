#include <string.h>
#include <stdio.h>
#include "ssd1306.h"
#include "ssd1306_tests.h"
#include "config.h"

#include "ssdFigure.h"

loadBar_t bar = {
    .x = 40,
    .y = 10,
    .width = 80,
    .height = 10,
};

void ssd1306_TestFonts() {
    char str[16];
    ssd1306_Fill(Black);
    ssd1306_SetCursor(2, 0);
    ssd1306_WriteString("Font 16x26", Font_16x26, White);
    ssd1306_SetCursor(2, 26);
    ssd1306_WriteString("Font 11x18", Font_11x18, White);
    ssd1306_SetCursor(2, 26+18);
    ssd1306_WriteString("Font 7x10", Font_7x10, White);
    ssd1306_SetCursor(2, 26+18+10);
    ssd1306_WriteString("Font 6x8", Font_6x8, White);
    ssd1306_UpdateScreen();

    vTaskDelay(MS2ST(500));
    ssd1306_SetCursor(0, 0);
    for (uint8_t i = 0; i < 100; i++)
    {
        bar.fill = i;
        sprintf(str, "%d", bar.fill);
        ssd1306_Fill(Black);
        ssdFigureDrawLoadBar(&bar);
        ssd1306_SetCursor(80, 25);
        ssd1306_WriteString(str, Font_7x10, White);
        drawMotor(2, 2);
        ssd1306_UpdateScreen();
        vTaskDelay(MS2ST(200));
    }
    
}


void ssd1306_TestAll() {
    ssd1306_Init();
    ssd1306_TestFonts();
}
