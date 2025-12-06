#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"

#if ENABLE_DISPLAY
#include "hardware/i2c.h"
#include "ssd1306.c"

static ssd1306_t display;
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
    ssd1306_clear(&display);

    // Draw "Hello World!" text
    ssd1306_draw_string(&display, 20, 10, "Hello World!", true);
    ssd1306_draw_rect(&display, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, true);
    ssd1306_display(&display);

    printf("Display initialized!\n");

    // Simple counter demo
    uint32_t counter = 0;
    char buf[32];

    while (true) {
        // Check for button press (active low)
        if (!gpio_get(BUTTON_PIN)) {
            counter = 0;
            sleep_ms(50);  // Simple debounce
        }

        // Update counter display
        ssd1306_fill_rect(&display, 20, 30, 100, 20, false);
        snprintf(buf, sizeof(buf), "Count: %lu", counter);
        ssd1306_draw_string(&display, 20, 35, buf, true);
        ssd1306_display(&display);

        counter++;
        sleep_ms(1000);
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
