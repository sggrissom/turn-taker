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

// `current` sits at the same offset it had in the two-name build, so a device
// updated in place comes back up on whoever it was already showing.
typedef struct {
    uint32_t magic;
    uint8_t current;
    uint8_t padding[3];
} save_data_t;

static void save_state(uint8_t current) {
    // Buffer must be FLASH_PAGE_SIZE (256 bytes) for flash_range_program
    uint8_t buffer[FLASH_PAGE_SIZE] = {0};
    save_data_t *data = (save_data_t*)buffer;
    data->magic = SAVE_MAGIC;
    data->current = current;

    // Must disable interrupts during flash operations
    uint32_t ints = save_and_disable_interrupts();

    // Erase the sector (required before writing)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    // Write the data (must be multiple of FLASH_PAGE_SIZE)
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);

    restore_interrupts(ints);
}

static bool load_state(uint8_t *current) {
    // Flash is memory-mapped, so we can read it directly
    const save_data_t *data = (const save_data_t *)(XIP_BASE + FLASH_TARGET_OFFSET);

    if (data->magic == SAVE_MAGIC) {
        *current = data->current;
        return true;
    }
    return false;
}

#if ENABLE_DISPLAY
#include "hardware/i2c.h"
#include "ssd1306.c"

static ssd1306_t display;

// Rotation order. Appending keeps the saved index meaning what it used to.
static const char *names[] = {"Maia", "Adalie", "Aria"};
static const uint8_t num_names = 3;

// UI constants
#define BORDER_MARGIN 2
#define LINE_MARGIN 8
#define NAME_SCALE 3
#define DOT_SIZE 5
#define DOT_SPACING 8

static uint8_t get_name_len(const char *name) {
    uint8_t len = 0;
    for (const char *p = name; *p; p++) len++;
    return len;
}

// Draw one person's screen at a horizontal offset (for animation): their name
// large and centred, who follows them, and a dot per person with theirs filled.
static void draw_content(uint8_t name_index, int16_t x_offset) {
    const char *name = names[name_index];
    int16_t text_width = get_name_len(name) * 6 * NAME_SCALE;
    int16_t text_height = 7 * NAME_SCALE;

    // Layout calculations
    int16_t line_y1 = 10;
    int16_t name_y = line_y1 + 6;
    int16_t line_y2 = name_y + text_height + 4;
    int16_t footer_y = line_y2 + 7;

    // Name, centred, with the offset applied
    int16_t name_x = (DISPLAY_WIDTH - text_width) / 2 + x_offset;
    ssd1306_draw_string_scaled(&display, name_x, name_y, name, NAME_SCALE, false);

    // Horizontal rules (black)
    ssd1306_draw_line(&display, LINE_MARGIN + x_offset, line_y1,
                      DISPLAY_WIDTH - LINE_MARGIN + x_offset, line_y1, false);
    ssd1306_draw_line(&display, LINE_MARGIN + x_offset, line_y2,
                      DISPLAY_WIDTH - LINE_MARGIN + x_offset, line_y2, false);

    // Footer left: who is up after this turn
    ssd1306_draw_string(&display, LINE_MARGIN + x_offset, footer_y, "next: ", false);
    ssd1306_draw_string(&display, LINE_MARGIN + 6 * 6 + x_offset, footer_y,
                        names[(name_index + 1) % num_names], false);

    // Footer right: position in the rotation, right-aligned to the rules
    int16_t dots_width = (num_names - 1) * DOT_SPACING + DOT_SIZE;
    int16_t dots_x = DISPLAY_WIDTH - LINE_MARGIN - dots_width + x_offset;
    int16_t dots_y = footer_y + 1;
    for (uint8_t i = 0; i < num_names; i++) {
        int16_t x = dots_x + i * DOT_SPACING;
        if (i == name_index) {
            ssd1306_fill_rect(&display, x, dots_y, DOT_SIZE, DOT_SIZE, false);
        } else {
            ssd1306_draw_rect(&display, x, dots_y, DOT_SIZE, DOT_SIZE, false);
        }
    }
}

// The white page and its border sit still while the content slides over them.
static void draw_frame(void) {
    ssd1306_fill_rect(&display, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, true);
    ssd1306_draw_rect(&display, BORDER_MARGIN, BORDER_MARGIN,
                      DISPLAY_WIDTH - 2 * BORDER_MARGIN,
                      DISPLAY_HEIGHT - 2 * BORDER_MARGIN, false);
}

static void draw_screen(uint8_t name_index) {
    draw_frame();
    draw_content(name_index, 0);
    ssd1306_display(&display);
}

// Slide from one person to the next. `forward` sends the old name out to the
// left (NEXT); going BACK runs the same motion mirrored, so an undo visibly
// rewinds rather than looking like another advance.
static void animate_transition(uint8_t old_index, uint8_t new_index, bool forward) {
    const int16_t steps = 12;
    const int16_t direction = forward ? 1 : -1;

    for (int16_t i = 1; i <= steps; i++) {
        // Scale by i/steps rather than a rounded-down step size, so the last
        // frame lands the incoming name exactly centred instead of snapping.
        int16_t offset = i * DISPLAY_WIDTH / steps * direction;

        draw_frame();
        draw_content(old_index, -offset);
        draw_content(new_index, direction * DISPLAY_WIDTH - offset);

        ssd1306_display(&display);
        sleep_ms(25);
    }

    // Final frame - ensure perfectly centered
    draw_screen(new_index);
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

    gpio_set_dormant_irq_enabled(NEXT_BUTTON_PIN, DORMANT_WAKE_EVENTS, true);
    gpio_set_dormant_irq_enabled(BACK_BUTTON_PIN, DORMANT_WAKE_EVENTS, true);

    xosc_dormant();  // Returns once a button is pressed

    gpio_set_dormant_irq_enabled(NEXT_BUTTON_PIN, DORMANT_WAKE_EVENTS, false);
    gpio_set_dormant_irq_enabled(BACK_BUTTON_PIN, DORMANT_WAKE_EVENTS, false);

    // Restore the PLLs and the 125 MHz system clock, then rebuild I2C, whose
    // baud rate divisors were computed against the old clk_peri.
    clocks_init();
    i2c_init(I2C_PORT, I2C_BAUDRATE);
}
#endif

static void wait_for_buttons_released(void) {
    while (!gpio_get(NEXT_BUTTON_PIN) || !gpio_get(BACK_BUTTON_PIN)) {
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
    while (gpio_get(NEXT_BUTTON_PIN) && gpio_get(BACK_BUTTON_PIN)) {
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
    gpio_init(NEXT_BUTTON_PIN);
    gpio_set_dir(NEXT_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(NEXT_BUTTON_PIN);

    gpio_init(BACK_BUTTON_PIN);
    gpio_set_dir(BACK_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BACK_BUTTON_PIN);

    // Small delay to let the display power up
    sleep_ms(100);

    // Initialize the display
    ssd1306_init(&display, I2C_PORT, DISPLAY_I2C_ADDR, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // Load saved state or use defaults
    uint8_t current = 0;
    if (!load_state(&current)) {
        current = 0;
    }
    // Validate loaded values
    if (current >= num_names) current = 0;

    draw_screen(current);

    bool next_was_pressed = false;
    bool back_was_pressed = false;
    absolute_time_t last_activity = get_absolute_time();

    while (true) {
        bool next_pressed = !gpio_get(NEXT_BUTTON_PIN);
        bool back_pressed = !gpio_get(BACK_BUTTON_PIN);

        if (next_pressed || back_pressed) {
            last_activity = get_absolute_time();
        }

        // Advance the rotation on button release
        if (next_was_pressed && !next_pressed) {
            uint8_t old = current;
            current = (current + 1) % num_names;
            animate_transition(old, current, true);
            save_state(current);
        }

        // Step back on button release, to undo a mis-press
        if (back_was_pressed && !back_pressed) {
            uint8_t old = current;
            current = (current + num_names - 1) % num_names;
            animate_transition(old, current, false);
            save_state(current);
        }

        next_was_pressed = next_pressed;
        back_was_pressed = back_pressed;

        if (absolute_time_diff_us(last_activity, get_absolute_time()) >
            (int64_t)IDLE_TIMEOUT_MS * 1000) {
            enter_sleep();
            draw_screen(current);
            // The press that woke us is not a turn - swallow it by waiting for
            // the release and clearing the edge state the handlers act on.
            wait_for_buttons_released();
            next_was_pressed = false;
            back_was_pressed = false;
            last_activity = get_absolute_time();
        }

        sleep_ms(20);  // Debounce delay
    }
#else
    // Hardware debug test - LED blink + button test
    // LED blinks slowly by default
    // Next button (GP15): fast blink while held
    // Back button (GP14): solid on while held
    // Both buttons: very fast strobe
    printf("Hardware debug test starting...\n");

    // Initialize buttons with internal pull-up
    gpio_init(NEXT_BUTTON_PIN);
    gpio_set_dir(NEXT_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(NEXT_BUTTON_PIN);

    gpio_init(BACK_BUTTON_PIN);
    gpio_set_dir(BACK_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BACK_BUTTON_PIN);

    uint32_t counter = 0;
    while (true) {
        bool next_pressed = !gpio_get(NEXT_BUTTON_PIN);  // GP15
        bool back_pressed = !gpio_get(BACK_BUTTON_PIN);  // GP14

        if (next_pressed && back_pressed) {
            // Both: very fast strobe (50ms)
            gpio_put(LED_PIN, (counter / 50) % 2);
        } else if (next_pressed) {
            // Next only: fast blink (100ms)
            gpio_put(LED_PIN, (counter / 100) % 2);
        } else if (back_pressed) {
            // Back only: solid on
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
