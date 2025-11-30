#!/bin/bash
# Upload en monitor script voor UNIFIED-LVGL9-Crypto_Monitor
# Gebruik: ./upload_and_monitor.sh [port] [baudrate]
# Bijvoorbeeld: ./upload_and_monitor.sh /dev/cu.usbserial-1410 115200
# Upload de code en start daarna automatisch de serial monitor

SKETCH_DIR="/Users/janpieterduhen/MEGA/@HOKUSAI/Arduino_nieuw/UNIFIED-LVGL9-Crypto_Monitor"
BAUDRATE="${2:-115200}"
PORT="$1"

cd "$SKETCH_DIR"

# Detecteer platform uit platform_config.h
PLATFORM_CONFIG="$SKETCH_DIR/platform_config.h"
PLATFORM_NAME="Unknown Platform"

if [ -f "$PLATFORM_CONFIG" ]; then
    if grep -q "^#define PLATFORM_TTGO" "$PLATFORM_CONFIG"; then
        PLATFORM_NAME="TTGO T-Display"
    elif grep -q "^#define PLATFORM_CYD24" "$PLATFORM_CONFIG"; then
        PLATFORM_NAME="CYD 2.4"
    elif grep -q "^#define PLATFORM_CYD28" "$PLATFORM_CONFIG"; then
        PLATFORM_NAME="CYD 2.8"
    fi
fi

# Eerst uploaden
echo "=== Step 1: Uploading ($PLATFORM_NAME) ==="
if [ -n "$PORT" ]; then
    ./upload.sh "$PORT"
    UPLOAD_EXIT_CODE=$?
else
    ./upload.sh
    UPLOAD_EXIT_CODE=$?
    # Als upload wordt overgeslagen, vraag om port voor monitor
    if [ $UPLOAD_EXIT_CODE -eq 0 ]; then
        echo ""
        echo "Upload was skipped. Please provide port for monitor:"
        arduino-cli board list | grep -E "Port|usb|serial" | grep -v "Bluetooth"
        echo ""
        read -p "Enter port (e.g., /dev/cu.usbserial-1420) or press Enter to skip monitor: " PORT
        if [ -z "$PORT" ]; then
            echo "Monitor skipped."
            exit 0
        fi
    fi
fi

if [ $UPLOAD_EXIT_CODE -ne 0 ]; then
    echo "❌ Upload failed, not starting monitor"
    exit 1
fi

# Wacht even zodat de ESP32 kan opstarten
echo ""
echo "=== Waiting 2 seconds for ESP32 to boot ==="
sleep 2

# Start serial monitor
echo ""
echo "=== Step 2: Starting Serial Monitor ==="
if [ -n "$PORT" ]; then
    ./monitor.sh "$PORT" "$BAUDRATE"
else
    ./monitor.sh "" "$BAUDRATE"
fi
