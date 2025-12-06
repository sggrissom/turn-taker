#!/bin/bash
set -e

cd "$(dirname "$0")"

# Build
echo "Building..."
mkdir -p build
cd build
cmake .. > /dev/null
make

# Wait for Pico in BOOTSEL mode
echo ""
echo "Waiting for Pico in BOOTSEL mode..."
while [ ! -b /dev/sdd1 ]; do
    sleep 0.5
done
sleep 1  # Extra delay for device to settle

# Mount and get mount point from output
echo "Mounting..."
MOUNT_OUTPUT=$(udisksctl mount -b /dev/sdd1 2>&1) || true
echo "$MOUNT_OUTPUT"

# Extract mount point from output like "Mounted /dev/sdd1 at /run/media/sgg/RPI-RP2"
MOUNT_POINT=$(echo "$MOUNT_OUTPUT" | grep -oP 'at \K/.*' | head -1)

if [ -z "$MOUNT_POINT" ] || [ ! -d "$MOUNT_POINT" ]; then
    # Fallback: check common locations
    for mp in /run/media/$USER/RPI-RP2 /media/$USER/RPI-RP2; do
        if [ -d "$mp" ]; then
            MOUNT_POINT="$mp"
            break
        fi
    done
fi

if [ -z "$MOUNT_POINT" ] || [ ! -d "$MOUNT_POINT" ]; then
    echo "Error: Could not find mount point"
    exit 1
fi

# Copy
echo "Flashing to $MOUNT_POINT..."
cp turn_taker.uf2 "$MOUNT_POINT/"
sync

echo "Done! Pico should reboot now."
