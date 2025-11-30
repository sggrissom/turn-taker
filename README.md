# Turn Taker - RP2040 + SSD1315 OLED Project

A Raspberry Pi Pico project with SSD1315/SSD1306 I2C OLED display support.

## Prerequisites

1. **Pico SDK** - Install the Raspberry Pi Pico SDK:
   ```bash
   git clone https://github.com/raspberrypi/pico-sdk.git
   cd pico-sdk
   git submodule update --init
   export PICO_SDK_PATH=$(pwd)
   ```

2. **ARM Toolchain**:
   ```bash
   # Arch Linux
   sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib

   # Ubuntu/Debian
   sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi
   ```

3. **CMake** (version 3.13+):
   ```bash
   # Arch Linux
   sudo pacman -S cmake

   # Ubuntu/Debian
   sudo apt install cmake
   ```

## Wiring

Connect the SSD1315 OLED display to the Pico:

| Pico Pin | GPIO | Display Pin | Description  |
|----------|------|-------------|--------------|
| 6        | GP4  | SDA         | I2C Data     |
| 7        | GP5  | SCL         | I2C Clock    |
| 36       | 3V3  | VCC         | Power (3.3V) |
| 38       | GND  | GND         | Ground       |

```
         ┌─────────────────┐
         │  Raspberry Pi   │
         │      Pico       │
         │                 │
    SDA ─┤ GP4 (pin 6)     │
    SCL ─┤ GP5 (pin 7)     │
    VCC ─┤ 3V3 (pin 36)    │
    GND ─┤ GND (pin 38)    │
         └─────────────────┘
```

## Building

1. Set the SDK path (if not already set):
   ```bash
   export PICO_SDK_PATH=/path/to/pico-sdk
   ```

2. Create build directory and compile:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

3. The build produces `turn_taker.uf2` in the build directory.

## Flashing

1. Hold the **BOOTSEL** button on the Pico while connecting USB
2. The Pico will appear as a USB mass storage device (RPI-RP2)
3. Copy `turn_taker.uf2` to the device:
   ```bash
   cp build/turn_taker.uf2 /run/media/$USER/RPI-RP2/
   ```
4. The Pico will automatically reboot and run the program

## Configuration

Edit `include/config.h` to change:
- I2C pins (default: GP4/GP5)
- Display dimensions (default: 128x64)
- I2C address (default: 0x3C)

## Display Driver API

```c
// Initialize display
ssd1306_init(&display, I2C_PORT, addr, width, height);

// Clear buffer
ssd1306_clear(&display);

// Drawing functions
ssd1306_draw_pixel(&display, x, y, color);
ssd1306_draw_line(&display, x0, y0, x1, y1, color);
ssd1306_draw_rect(&display, x, y, w, h, color);
ssd1306_fill_rect(&display, x, y, w, h, color);
ssd1306_draw_char(&display, x, y, 'A', color);
ssd1306_draw_string(&display, x, y, "Hello", color);

// Send buffer to display
ssd1306_display(&display);

// Other controls
ssd1306_set_contrast(&display, 0xFF);
ssd1306_invert(&display, true);
```

## USB Serial Debug

The project is configured to output to USB serial. Connect via:
```bash
# Find the device
ls /dev/ttyACM*

# Connect (e.g., using minicom)
minicom -D /dev/ttyACM0 -b 115200
```
