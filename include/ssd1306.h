#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

// Display dimensions (can be overridden)
#ifndef SSD1306_WIDTH
#define SSD1306_WIDTH 128
#endif

#ifndef SSD1306_HEIGHT
#define SSD1306_HEIGHT 64
#endif

// Buffer size for the display
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

// SSD1306 display structure
typedef struct {
    i2c_inst_t *i2c;
    uint8_t addr;
    uint8_t width;
    uint8_t height;
    uint8_t buffer[SSD1306_BUFFER_SIZE];
} ssd1306_t;

// Initialization and control
static void ssd1306_init(ssd1306_t *display, i2c_inst_t *i2c, uint8_t addr, uint8_t width, uint8_t height);
static void ssd1306_display(ssd1306_t *display);
static void ssd1306_clear(ssd1306_t *display);
static void ssd1306_set_contrast(ssd1306_t *display, uint8_t contrast);
static void ssd1306_invert(ssd1306_t *display, bool invert);

// Drawing primitives
static void ssd1306_draw_pixel(ssd1306_t *display, int16_t x, int16_t y, bool color);
static void ssd1306_draw_line(ssd1306_t *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color);
static void ssd1306_draw_rect(ssd1306_t *display, int16_t x, int16_t y, int16_t w, int16_t h, bool color);
static void ssd1306_fill_rect(ssd1306_t *display, int16_t x, int16_t y, int16_t w, int16_t h, bool color);

// Text rendering
static void ssd1306_draw_char(ssd1306_t *display, int16_t x, int16_t y, char c, bool color);
static void ssd1306_draw_string(ssd1306_t *display, int16_t x, int16_t y, const char *str, bool color);

#endif // SSD1306_H
