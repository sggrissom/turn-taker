#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "config.h"
#include "ssd1306.h"

static ssd1306_t display;

int main() {
    // Initialize stdio for USB serial debugging
    stdio_init_all();

    // Initialize I2C
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Small delay to let the display power up
    sleep_ms(100);

    // Initialize the display
    ssd1306_init(&display, I2C_PORT, DISPLAY_I2C_ADDR, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // Clear the display
    ssd1306_clear(&display);

    // Draw "Hello World!" text
    ssd1306_draw_string(&display, 20, 10, "Hello World!", true);

    // Draw a border around the display
    ssd1306_draw_rect(&display, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, true);

    // Update the display
    ssd1306_display(&display);

    printf("Display initialized!\n");

    // Simple counter demo
    uint32_t counter = 0;
    char buf[32];

    while (true) {
        // Update counter display
        ssd1306_fill_rect(&display, 20, 30, 100, 20, false);  // Clear area
        snprintf(buf, sizeof(buf), "Count: %lu", counter);
        ssd1306_draw_string(&display, 20, 35, buf, true);
        ssd1306_display(&display);

        counter++;
        sleep_ms(1000);
    }

    return 0;
}
