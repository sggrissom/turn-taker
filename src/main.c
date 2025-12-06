#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"

#if ENABLE_DISPLAY
#include "hardware/i2c.h"
#include "ssd1306.c"

static ssd1306_t display;

// Names to display
static const char *names[] = {"Maia", "Adalie"};
static const uint8_t num_names = 2;

static void draw_name(uint8_t index) {
    const char *name = names[index];
    uint8_t scale = 3;

    // Calculate text width: chars * (5 pixels + 1 spacing) * scale
    uint8_t len = 0;
    for (const char *p = name; *p; p++) len++;
    int16_t text_width = len * 6 * scale;
    int16_t text_height = 7 * scale;

    // Center on display
    int16_t x = (DISPLAY_WIDTH - text_width) / 2;
    int16_t y = (DISPLAY_HEIGHT - text_height) / 2;

    ssd1306_clear(&display);
    ssd1306_draw_string_scaled(&display, x, y, name, scale, true);
    ssd1306_display(&display);
}
#endif

int main() {
    stdio_init_all();

    // LED setup - quick blink to show code is running
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    sleep_ms(100);
    gpio_put(LED_PIN, 0);

#if ENABLE_DISPLAY
    // Initialize I2C
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Initialize button with internal pull-up
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // Small delay to let the display power up
    sleep_ms(100);

    // Initialize the display
    ssd1306_init(&display, I2C_PORT, DISPLAY_I2C_ADDR, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // Show first name
    uint8_t current = 0;
    draw_name(current);

    bool button_was_pressed = false;

    while (true) {
        bool button_pressed = !gpio_get(BUTTON_PIN);

        // Toggle on button release (falling edge)
        if (button_was_pressed && !button_pressed) {
            current = (current + 1) % num_names;
            draw_name(current);
        }

        button_was_pressed = button_pressed;
        sleep_ms(20);  // Debounce delay
    }
#else
    // Simple LED blink test
    printf("LED blink test starting...\n");

    bool led_state = false;
    while (true) {
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);
        sleep_ms(500);
    }
#endif

    return 0;
}
