# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Deploy Commands

```bash
# Build
mkdir -p build && cd build && cmake .. && make

# Deploy (builds, waits for Pico in BOOTSEL mode, flashes)
./deploy.sh

# Manual flash (after build)
# Hold BOOTSEL on Pico, connect USB, then:
cp build/turn_taker.uf2 /run/media/$USER/RPI-RP2/
```

## Architecture

This is an RP2040 (Raspberry Pi Pico) project using the Pico SDK. It displays a turn-taking UI on an SSD1315 OLED display.

### Key Files
- `src/main.c` - Application logic, UI rendering, flash persistence
- `src/ssd1306.c` - Display driver (included via unity build in main.c)
- `include/config.h` - Pin assignments, feature flags, display configuration
- `include/ssd1306.h` - Display driver header

### Hardware Configuration
- **Display**: SSD1315/SSD1306 128x64 OLED via I2C (GP4=SDA, GP5=SCL)
- **Display VCC**: Requires 5V (VBUS pin 40), not 3.3V
- **Take button**: GP15 (pin 20) - uses turn
- **Defer button**: GP14 (pin 19) - adds turn
- **Onboard LED**: GP25

### Flash Persistence
State is saved to the last 4KB sector of flash. Important constraints:
- `flash_range_program()` requires 256-byte aligned buffer (FLASH_PAGE_SIZE)
- `flash_range_erase()` requires 4KB sector size (FLASH_SECTOR_SIZE)
- Interrupts must be disabled during flash operations

### Display Driver
Uses inverted colors (white background, black text). Key scaled text function:
- `ssd1306_draw_string_scaled()` - Renders text at configurable scale (default scale 3 = 15x21 pixel chars)

### Feature Flag
Set `ENABLE_DISPLAY 0` in config.h to build LED-only blink test (useful for hardware debugging).
