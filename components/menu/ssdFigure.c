#include "ssdFigure.h"
#include "ssd1306.h"

#include "math.h"

//#undef LOG
//#define LOG(...) //LOG( __VA_ARGS__)

bool bitmap[] = 
{
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 
0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 
1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 
1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 
1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 
1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 
1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 
0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 

};

bool circle[] = 
{
	0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
	0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };

bool battery[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0,
	0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0,
	0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0,
	0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0,
	0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int ssdFigureDrawLoadBar(loadBar_t * figure)
{
	if (figure->width + figure->x > SSD1306_WIDTH) return FALSE;
	if (figure->height + figure->y > SSD1306_HEIGHT) return FALSE;
	if (figure->fill > 100) figure->fill = 100;

	int scaling_fill_x = figure->width * figure->fill / 100 + figure->x;
	for (int x = figure->x; x < figure->width + figure->x; x++)
	{
		for (int y = figure->y; y < figure->height + figure->y; y++)
		{
			if (x <= scaling_fill_x || x == figure->width + figure->x - 1) {
				ssd1306_DrawPixel(x, y, (SSD1306_COLOR) White);
			}
			else if (y == figure->y || y == figure->height + figure->y - 1) {
				ssd1306_DrawPixel(x, y, (SSD1306_COLOR) White);
			}
		}
	}
	return TRUE;
}

int ssdFigureDrawScrollBar(scrollBar_t * figure)
{
	if (figure == NULL) return FALSE;
	if (figure->y_start > SSD1306_HEIGHT) return FALSE;
	if (figure->all_line == 0) return FALSE;
	float width = (float)figure->line_max / ((float)figure->all_line);
	if (width >= 1.0) return FALSE;
	int width_px = width * (SSD1306_HEIGHT - figure->y_start);
	int step = (SSD1306_HEIGHT - figure->y_start - width_px)/figure->all_line; 
	int start_scroll_y = step * (figure->actual_line + 1) + figure->y_start;
	//LOG("width_px %d, step %d, start_scroll_y %d\n", width_px, step, start_scroll_y);
	for (int x = SSD1306_WIDTH - 4; x < SSD1306_WIDTH; x++)
	{
		for (int y = figure->y_start; y < SSD1306_HEIGHT; y++)
		{
			if (x <= SSD1306_WIDTH - 4 || x == SSD1306_WIDTH - 1) {
				ssd1306_DrawPixel(x, y, (SSD1306_COLOR) White);
				continue;
			}
			else if (y == figure->y_start || (y >= start_scroll_y && y <=start_scroll_y + width_px) || y == SSD1306_HEIGHT)  {
				ssd1306_DrawPixel(x, y, (SSD1306_COLOR) White);
				continue;
			}
			ssd1306_DrawPixel(x, y, (SSD1306_COLOR) Black);
		}
	}
	return TRUE;
}

int ssdFigureFillLine(int y_start, int height)
{
	for(int y = y_start; y <= y_start + height; y++)
	{
		for (int x = 0; x < SSD1306_WIDTH; x++)
		{
			ssd1306_DrawPixel(x, y, (SSD1306_COLOR) White);
		}
	}
	return TRUE;
}

void drawMotor(uint8_t x, uint8_t y)
{
	for (int i = 0; i < 30; i++)
	{
		for (int j = 0; j < 23; j++)
		{
			if (bitmap[j*30 + i])
				ssd1306_DrawPixel(i + x, j + y, (SSD1306_COLOR) White);
		}
	}
}

#define DIAMETR 22
void drawServo(uint8_t x, uint8_t y, uint8_t open)
{
	uint8_t x_open = x + DIAMETR * open / 100;
	uint8_t start_flag;
	for (int i = 0; i < 22; i++)
	{
		start_flag = 0;
		for (int j = 0; j < 22; j++)
		{
			if (circle[j*22 + i])
			{
				if (start_flag == 0){
					//LOG("find_start\n", x_open);
					start_flag = 1;
				}
				else {
					//LOG("find_end\n", x_open);
					start_flag = 0;
				}
				ssd1306_DrawPixel(i + x, j + y, (SSD1306_COLOR) White);
			}
			if (start_flag == 1 && i + x > x_open)
				ssd1306_DrawPixel(i + x, j + y, (SSD1306_COLOR) White);
				
		}
	}
}

typedef enum
{
	BATTERY_NORMAL,
	BATTERY_CHARGING,
	BATTERY_LOW_VOLTAGE,
}battery_state_t;

#define ANIMATION_TIMEOUT 500

static uint32_t animation_timer;
static uint32_t animation_cnt;

static void animation_counter_process(void)
{
	uint32_t time_now = xTaskGetTickCount();
	if (animation_timer < time_now)
	{
		animation_timer = time_now + MS2ST(ANIMATION_TIMEOUT);
		animation_cnt++;
	}
}

static void _drawBattery(uint8_t x, uint8_t y, uint8_t chrg)
{
	for (int j = 0; j < 8; j++)
	{
		for (int i = 0; i < 11; i++)
		{
			if (battery[j*11 + i])
			{
				ssd1306_DrawPixel(i + x, j + y, (SSD1306_COLOR) White);
			}

			if (j > 1 && j < 6 && i > 1 && i < 1 + chrg) {
				ssd1306_DrawPixel(i + x, j + y, (SSD1306_COLOR) White);
			}	
		}
	}
}

void drawBattery(uint8_t x, uint8_t y, float accum_voltage, bool is_charging)
{
	animation_counter_process();
	uint8_t x_charge = 0;
	if (accum_voltage < 3.2)
	{
		x_charge = 0;
	}
	else
	{
		x_charge = (uint8_t)((float)(accum_voltage - 3.2) * 8.0);

		if (x_charge > 7) {
			x_charge = 7;
		}
	}

	battery_state_t state = BATTERY_NORMAL;

	if (accum_voltage < 3.48)
	{
		state = BATTERY_LOW_VOLTAGE;
	}

	if (is_charging)
	{
		state = BATTERY_CHARGING;
	}

	switch (state)
	{
		case BATTERY_NORMAL:
			_drawBattery(x, y, x_charge);
		break;

		case BATTERY_CHARGING:
			_drawBattery(x, y, x_charge + animation_cnt % (8 - x_charge));
		break;

		case BATTERY_LOW_VOLTAGE:
			if (animation_cnt % 2)
			{
				_drawBattery(x, y, x_charge);
			}
		break;
	
	default:
		_drawBattery(x, y, x_charge);
		break;
	}	
	//LOG("x %d, cnt %d", x_charge, animation_cnt);
}