#ifndef CONFIG_H
#define CONFIG_H

// Feature flags
#define ENABLE_DISPLAY 0

// LED Configuration (GP25 is the onboard LED on Pico)
#define LED_PIN 25

// I2C Configuration
#define I2C_PORT i2c0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define I2C_BAUDRATE 400000  // 400 kHz

// SSD1315/SSD1306 Display Configuration
// Note: Display VCC needs 5V (VBUS pin 40), not 3.3V
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_I2C_ADDR 0x3C

// Button Configuration
#define BUTTON_PIN 15        // Take turn (GP15, pin 20)
#define DEFER_BUTTON_PIN 14  // Defer/add turn (GP14, pin 19)

#endif // CONFIG_H
