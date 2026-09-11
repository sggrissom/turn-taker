cmake_minimum_required(VERSION 3.13)

# Include the Pico SDK import script
include(pico_sdk_import.cmake)

project(turn_taker C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Add the main executable
add_executable(turn_taker
    src/main.c
)

# Include directories
target_include_directories(turn_taker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(turn_taker
    pico_stdlib
    hardware_i2c
    hardware_flash
    hardware_sync
    hardware_clocks
    hardware_pll
    hardware_xosc
)

# USB serial costs several mA of permanent idle draw and keeps the USB PHY and
# clocks alive, which is incompatible with the dormant sleep in config.h. It is
# off by default; for debugging build with:
#   cmake -DENABLE_USB_STDIO=ON .. (and set ENABLE_DEEP_SLEEP 0 in config.h)
option(ENABLE_USB_STDIO "Enable USB serial stdio (raises idle power draw)" OFF)
if (ENABLE_USB_STDIO)
    pico_enable_stdio_usb(turn_taker 1)
else ()
    pico_enable_stdio_usb(turn_taker 0)
endif ()
pico_enable_stdio_uart(turn_taker 0)

# Create UF2 file for flashing
pico_add_extra_outputs(turn_taker)
