#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "config.h"

// Flash storage for persistent state
// Use last sector of 2MB flash (offset from start of flash)
#define FLASH_TARGET_OFFSET (2 * 1024 * 1024 - FLASH_SECTOR_SIZE)
#define SAVE_MAGIC 0x5455524E  // "TURN" in hex

typedef struct {
    uint32_t magic;
    uint8_t current;
    uint8_t turns;
    uint8_t padding[2];
} save_data_t;

static void save_state(uint8_t current, uint8_t turns) {
    // Buffer must be FLASH_PAGE_SIZE (256 bytes) for flash_range_program
    uint8_t buffer[FLASH_PAGE_SIZE] = {0};
    save_data_t *data = (save_data_t*)buffer;
    data->magic = SAVE_MAGIC;
    data->current = current;
    data->turns = turns;

    // Must disable interrupts during flash operations
    uint32_t ints = save_and_disable_interrupts();

    // Erase the sector (required before writing)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    // Write the data (must be multiple of FLASH_PAGE_SIZE)
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);

    restore_interrupts(ints);
}

static bool load_state(uint8_t *current, uint8_t *turns) {
    // Flash is memory-mapped, so we can read it directly
    const save_data_t *data = (const save_data_t *)(XIP_BASE + FLASH_TARGET_OFFSET);

    if (data->magic == SAVE_MAGIC) {
        *current = data->current;
        *turns = data->turns;
        return true;
    }
    return false;
}

#if ENABLE_DISPLAY
#include "hardware/i2c.h"
#include "ssd1306.c"

static ssd1306_t display;

// Names to display
static const char *names[] = {"Maia", "Adalie"};
static const uint8_t num_names = 2;

// UI constants
#define BORDER_MARGIN 2
#define LINE_MARGIN 8
#define NAME_SCALE 3

static uint8_t get_name_len(const char *name) {
    uint8_t len = 0;
    for (const char *p = name; *p; p++) len++;
    return len;
}

// Draw screen content at a horizontal offset (for animation)
static void draw_content(const char *name, uint8_t turns, int16_t x_offset) {
    uint8_t len = get_name_len(name);
    int16_t text_width = len * 6 * NAME_SCALE;
    int16_t text_height = 7 * NAME_SCALE;

    // Dot parameters
    uint8_t dot_size = 6;
    uint8_t dot_spacing = 10;
    int16_t dots_width = turns * dot_size + (turns - 1) * (dot_spacing - dot_size);
    int16_t gap = 6;

    // Layout calculations
    int16_t line_y1 = 10;
    int16_t name_y = line_y1 + 6;
    int16_t line_y2 = name_y + text_height + 4;
    int16_t dots_y = line_y2 + 8;

    // Center name horizontally with offset
    int16_t name_x = (DISPLAY_WIDTH - text_width) / 2 + x_offset;
    int16_t dots_x = (DISPLAY_WIDTH - dots_width) / 2 + x_offset;

    // Draw name (black on white = false)
    ssd1306_draw_string_scaled(&display, name_x, name_y, name, NAME_SCALE, false);

    // Draw horizontal lines (black)
    ssd1306_draw_line(&display, LINE_MARGIN + x_offset, line_y1,
                      DISPLAY_WIDTH - LINE_MARGIN + x_offset, line_y1, false);
    ssd1306_draw_line(&display, LINE_MARGIN + x_offset, line_y2,
                      DISPLAY_WIDTH - LINE_MARGIN + x_offset, line_y2, false);

    // Draw dots (black)
    for (uint8_t i = 0; i < turns; i++) {
        int16_t x = dots_x + i * dot_spacing;
        ssd1306_fill_rect(&display, x, dots_y, dot_size, dot_size, false);
    }
}

static void draw_screen(uint8_t name_index, uint8_t turns) {
    // Fill white background
    ssd1306_fill_rect(&display, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, true);

    // Draw black border
    ssd1306_draw_rect(&display, BORDER_MARGIN, BORDER_MARGIN,
                      DISPLAY_WIDTH - 2 * BORDER_MARGIN,
                      DISPLAY_HEIGHT - 2 * BORDER_MARGIN, false);

    // Draw content
    draw_content(names[name_index], turns, 0);

    ssd1306_display(&display);
}

static void animate_transition(uint8_t old_index, uint8_t new_index, uint8_t turns) {
    const int16_t steps = 12;
    const int16_t step_size = DISPLAY_WIDTH / steps;

    for (int16_t i = 1; i <= steps; i++) {
        int16_t offset = i * step_size;

        // Fill white background
        ssd1306_fill_rect(&display, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, true);

        // Draw black border (stays fixed)
        ssd1306_draw_rect(&display, BORDER_MARGIN, BORDER_MARGIN,
                          DISPLAY_WIDTH - 2 * BORDER_MARGIN,
                          DISPLAY_HEIGHT - 2 * BORDER_MARGIN, false);

        // Old name slides out to the left
        draw_content(names[old_index], 1, -offset);

        // New name slides in from the right
        draw_content(names[new_index], turns, DISPLAY_WIDTH - offset);

        ssd1306_display(&display);
        sleep_ms(25);
    }

    // Final frame - ensure perfectly centered
    draw_screen(new_index, turns);
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

    // Load saved state or use defaults
    uint8_t current = 0;
    uint8_t turns = 1;
    if (!load_state(&current, &turns)) {
        // No valid save, use defaults
        current = 0;
        turns = 1;
    }
    // Validate loaded values
    if (current >= num_names) current = 0;
    if (turns < 1 || turns > 3) turns = 1;

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
                // Next person's turn - animate transition
                uint8_t old = current;
                current = (current + 1) % num_names;
                turns = 1;
                animate_transition(old, current, turns);
            } else {
                draw_screen(current, turns);
            }
            save_state(current, turns);
        }

        // Defer (add a turn) on button release
        if (defer_was_pressed && !defer_pressed) {
            if (turns < 3) {
                turns++;
                draw_screen(current, turns);
                save_state(current, turns);
            }
        }

        take_was_pressed = take_pressed;
        defer_was_pressed = defer_pressed;
        sleep_ms(20);  // Debounce delay
    }
#else
    // Hardware debug test - LED blink + button test
    // LED blinks slowly by default
    // Take button (GP15): fast blink while held
    // Defer button (GP14): solid on while held
    // Both buttons: very fast strobe
    printf("Hardware debug test starting...\n");

    // Initialize buttons with internal pull-up
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    gpio_init(DEFER_BUTTON_PIN);
    gpio_set_dir(DEFER_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(DEFER_BUTTON_PIN);

    uint32_t counter = 0;
    while (true) {
        bool take_pressed = !gpio_get(BUTTON_PIN);      // GP15
        bool defer_pressed = !gpio_get(DEFER_BUTTON_PIN); // GP14

        if (take_pressed && defer_pressed) {
            // Both: very fast strobe (50ms)
            gpio_put(LED_PIN, (counter / 50) % 2);
        } else if (take_pressed) {
            // Take only: fast blink (100ms)
            gpio_put(LED_PIN, (counter / 100) % 2);
        } else if (defer_pressed) {
            // Defer only: solid on
            gpio_put(LED_PIN, 1);
        } else {
            // No buttons: slow blink (500ms)
            gpio_put(LED_PIN, (counter / 500) % 2);
        }

        sleep_ms(1);
        counter++;
    }
#endif

    return 0;
}
