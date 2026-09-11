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

#if ENABLE_DEEP_SLEEP
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/xosc.h"
#include "pico/runtime_init.h"

// Level-sensitive wake, not edge: the GPIO edge detector is clocked from
// clk_sys, which is stopped while dormant.
#define DORMANT_WAKE_EVENTS GPIO_IRQ_LEVEL_LOW

// Put every clock on the crystal so nothing is left running from a PLL when the
// PLLs stop. clk_sys in particular must not be PLL-derived or the core will not
// come back when the crystal restarts.
static void clocks_run_from_xosc(void) {
    clock_configure(clk_ref, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, 0,
                    XOSC_HZ, XOSC_HZ);
    clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, 0,
                    XOSC_HZ, XOSC_HZ);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    XOSC_HZ, XOSC_HZ);
    clock_stop(clk_usb);
    clock_stop(clk_adc);
    clock_stop(clk_rtc);
    pll_deinit(pll_sys);
    pll_deinit(pll_usb);
}

// Halt the RP2040 until a button pulls its pin low. ROSC is deliberately left
// running: clocks_init() briefly parks clk_ref on it while rebuilding the PLLs.
static void enter_dormant(void) {
    clocks_run_from_xosc();

    gpio_set_dormant_irq_enabled(BUTTON_PIN, DORMANT_WAKE_EVENTS, true);
    gpio_set_dormant_irq_enabled(DEFER_BUTTON_PIN, DORMANT_WAKE_EVENTS, true);

    xosc_dormant();  // Returns once a button is pressed

    gpio_set_dormant_irq_enabled(BUTTON_PIN, DORMANT_WAKE_EVENTS, false);
    gpio_set_dormant_irq_enabled(DEFER_BUTTON_PIN, DORMANT_WAKE_EVENTS, false);

    // Restore the PLLs and the 125 MHz system clock, then rebuild I2C, whose
    // baud rate divisors were computed against the old clk_peri.
    clocks_init();
    i2c_init(I2C_PORT, I2C_BAUDRATE);
}
#endif

static void wait_for_buttons_released(void) {
    while (!gpio_get(BUTTON_PIN) || !gpio_get(DEFER_BUTTON_PIN)) {
        sleep_ms(20);
    }
    sleep_ms(20);  // Debounce the release
}

// Blank the panel and idle until a button is pressed. Returns with the display
// lit again and the caller responsible for redrawing.
static void enter_sleep(void) {
    ssd1306_sleep(&display);

#if ENABLE_DEEP_SLEEP
    enter_dormant();
    // The module never lost power, but its controller comes back with the
    // charge pump down and the clocks it was configured against changed, so
    // bring it up through a full init rather than a bare display-on.
    ssd1306_init(&display, I2C_PORT, DISPLAY_I2C_ADDR, DISPLAY_WIDTH, DISPLAY_HEIGHT);
#else
    while (gpio_get(BUTTON_PIN) && gpio_get(DEFER_BUTTON_PIN)) {
        sleep_ms(20);
    }
    ssd1306_wake(&display);
#endif
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
    absolute_time_t last_activity = get_absolute_time();

    while (true) {
        bool take_pressed = !gpio_get(BUTTON_PIN);
        bool defer_pressed = !gpio_get(DEFER_BUTTON_PIN);

        if (take_pressed || defer_pressed) {
            last_activity = get_absolute_time();
        }

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

        if (absolute_time_diff_us(last_activity, get_absolute_time()) >
            (int64_t)IDLE_TIMEOUT_MS * 1000) {
            enter_sleep();
            draw_screen(current, turns);
            // The press that woke us is not a turn - swallow it by waiting for
            // the release and clearing the edge state the handlers act on.
            wait_for_buttons_released();
            take_was_pressed = false;
            defer_was_pressed = false;
            last_activity = get_absolute_time();
        }

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
