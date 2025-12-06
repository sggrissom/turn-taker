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

static void draw_screen(uint8_t name_index, uint8_t turns) {
    const char *name = names[name_index];
    uint8_t scale = 3;

    // Calculate text dimensions
    uint8_t len = 0;
    for (const char *p = name; *p; p++) len++;
    int16_t text_width = len * 6 * scale;
    int16_t text_height = 7 * scale;

    // Dot parameters
    uint8_t dot_size = 6;
    uint8_t dot_spacing = 10;  // center-to-center
    int16_t dots_width = turns * dot_size + (turns - 1) * (dot_spacing - dot_size);
    int16_t gap = 8;  // gap between name and dots

    // Total height of name + gap + dots
    int16_t total_height = text_height + gap + dot_size;

    // Center vertically
    int16_t name_y = (DISPLAY_HEIGHT - total_height) / 2;
    int16_t dots_y = name_y + text_height + gap;

    // Center name horizontally
    int16_t name_x = (DISPLAY_WIDTH - text_width) / 2;

    // Center dots horizontally
    int16_t dots_x = (DISPLAY_WIDTH - dots_width) / 2;

    ssd1306_clear(&display);
    ssd1306_draw_string_scaled(&display, name_x, name_y, name, scale, true);

    // Draw dots
    for (uint8_t i = 0; i < turns; i++) {
        int16_t x = dots_x + i * dot_spacing;
        ssd1306_fill_rect(&display, x, dots_y, dot_size, dot_size, true);
    }

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

    // Initialize buttons with internal pull-up
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    gpio_init(DEFER_BUTTON_PIN);
    gpio_set_dir(DEFER_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(DEFER_BUTTON_PIN);

    // Small delay to let the display power up
    sleep_ms(100);

    // Initialize the display
    ssd1306_init(&display, I2C_PORT, DISPLAY_I2C_ADDR, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // Game state
    uint8_t current = 0;
    uint8_t turns = 1;

    draw_screen(current, turns);

    bool take_was_pressed = false;
    bool defer_was_pressed = false;

    while (true) {
        bool take_pressed = !gpio_get(BUTTON_PIN);
        bool defer_pressed = !gpio_get(DEFER_BUTTON_PIN);

        // Take a turn on button release
        if (take_was_pressed && !take_pressed) {
            turns--;
            if (turns == 0) {
                // Next person's turn
                current = (current + 1) % num_names;
                turns = 1;
            }
            draw_screen(current, turns);
        }

        // Defer (add a turn) on button release
        if (defer_was_pressed && !defer_pressed) {
            if (turns < 3) {
                turns++;
                draw_screen(current, turns);
            }
        }

        take_was_pressed = take_pressed;
        defer_was_pressed = defer_pressed;
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
