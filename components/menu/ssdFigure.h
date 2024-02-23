#ifndef SSD_FIGURE_H
#define SSD_FIGURE_H
#include "app_config.h"

typedef struct ssdFigure
{
  uint8_t x;
  uint8_t y;
  uint8_t height;
  uint8_t width;
  uint8_t fill;
} loadBar_t;

typedef struct
{
  uint8_t y_start;
  uint8_t line_max;
  uint8_t all_line;
  uint8_t actual_line;
} scrollBar_t;

int ssdFigureDrawLoadBar( loadBar_t* figure );
int ssdFigureDrawScrollBar( scrollBar_t* figure );
int ssdFigureFillLine( int y_start, int height );
void drawMotor( uint8_t x, uint8_t y );
void drawMotorCircle( uint8_t x, uint8_t y, uint8_t cnt );
void drawVibro( uint8_t x, uint8_t y, uint8_t cnt );
void drawServo( uint8_t x, uint8_t y, uint8_t open );
void drawBattery( uint8_t x, uint8_t y, float accum_voltage, bool is_charging );
void drawSignal( uint8_t x, uint8_t y, uint8_t signal_lvl );
void drawQR( uint8_t x, uint8_t y );
void draw_low_accu( uint8_t x, uint8_t y, float acc_voltage, float acc_current );

#endif