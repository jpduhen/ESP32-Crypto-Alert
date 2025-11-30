#!/bin/bash
# Upload script voor UNIFIED-LVGL9-Crypto_Monitor
# Gebruik: ./upload.sh [port]
# Bijvoorbeeld: ./upload.sh /dev/cu.usbserial-1410

SKETCH_DIR="/Users/janpieterduhen/MEGA/@HOKUSAI/Arduino_nieuw/UNIFIED-LVGL9-Crypto_Monitor"
SKETCH_NAME="UNIFIED-LVGL9-Crypto_Monitor"
FQBN="esp32:esp32:esp32:UploadSpeed=460800,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,DebugLevel=none,PSRAM=disabled,LoopCore=1,EventsCore=1,EraseFlash=none"

cd "$SKETCH_DIR"

# Detecteer platform uit platform_config.h
PLATFORM_CONFIG="$SKETCH_DIR/platform_config.h"
PLATFORM_NAME="Unknown Platform"

if [ -f "$PLATFORM_CONFIG" ]; then
    if grep -q "^#define PLATFORM_TTGO" "$PLATFORM_CONFIG"; then
        PLATFORM_NAME="TTGO T-Display"
    elif grep -q "^#define PLATFORM_CYD28" "$PLATFORM_CONFIG"; then
        PLATFORM_NAME="CYD 2.8"
    fi
fi

echo "=== Compiling $SKETCH_NAME ($PLATFORM_NAME) ==="
arduino-cli compile \
    --fqbn "$FQBN" \
    "$SKETCH_DIR"

if [ $? -ne 0 ]; then
    echo "❌ Compilation failed!"
    exit 1
fi

echo ""
echo "✅ Compilation successful!"
echo ""

# Als port als argument is gegeven, gebruik die
if [ -n "$1" ]; then
    PORT="$1"
else
    echo "Available ports:"
    arduino-cli board list | grep -E "Port|usb|serial" | grep -v "Bluetooth"
    echo ""
    read -p "Enter port (e.g., /dev/cu.usbserial-1410) or press Enter to skip upload: " PORT
fi

if [ -n "$PORT" ]; then
    echo ""
    echo "=== Uploading to $PORT ($PLATFORM_NAME) ==="
    arduino-cli upload \
        --fqbn "$FQBN" \
        --port "$PORT" \
        "$SKETCH_DIR"
    
    if [ $? -eq 0 ]; then
        echo "✅ Upload successful!"
    else
        echo "❌ Upload failed!"
        exit 1
    fi
else
    echo "Upload skipped."
fi
