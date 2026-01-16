#!/bin/bash
# Build + upload + serial monitor script voor UNIFIED-LVGL9-Crypto_Monitor
# Gebruik: ./monitor.sh [port] [baudrate] [duur_sec] [fqbn] [logfile]
# Bijvoorbeeld: ./monitor.sh /dev/cu.usbserial-1410 115200 180 esp32:esp32:cyd28
# Output wordt opgeslagen in logs/serial-YYYYMMDD-HHMMSS.log (alleen leesbare tekst)

SKETCH_DIR="/Users/janpieterduhen/MEGA/@HOKUSAI/Arduino_nieuw/UNIFIED-LVGL9-Crypto_Monitor"
BAUDRATE="${2:-115200}"  # Default 115200 als niet opgegeven
DURATION_SEC="${3:-0}"   # 0 = oneindig
FQBN="${4:-}"

TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="$SKETCH_DIR/logs"
DEFAULT_LOG_FILE="$LOG_DIR/serial-$TIMESTAMP.log"
LOG_FILE="${5:-$DEFAULT_LOG_FILE}"

cd "$SKETCH_DIR"
mkdir -p "$LOG_DIR"

# Als port als argument is gegeven, gebruik die
if [ -n "$1" ]; then
    PORT="$1"
else
    echo "Available ports:"
    arduino-cli board list | grep -E "Port|usb|serial" | grep -v "Bluetooth"
    echo ""
    read -p "Enter port (e.g., /dev/cu.usbserial-1410): " PORT
fi

if [ -z "$PORT" ]; then
    echo "❌ No port specified!"
    exit 1
fi

if [ -z "$FQBN" ]; then
    echo "Available boards (hint):"
    filtered_list="$(arduino-cli board listall | grep -i -E "cyd|2432|tft|display" | head -n 40)"
    if [ -n "$filtered_list" ]; then
        echo "$filtered_list"
    else
        arduino-cli board listall | head -n 20
    fi
    echo ""
    read -p "Enter FQBN (e.g., esp32:esp32:cyd28): " FQBN
fi

if [ -z "$FQBN" ]; then
    echo "❌ No FQBN specified!"
    exit 1
fi

echo "=== Build + Upload ==="
echo "FQBN: $FQBN"
echo "Sketch: $SKETCH_DIR"
echo ""

if ! arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR"; then
    echo "❌ Compile failed!"
    exit 1
fi

if ! arduino-cli upload --fqbn "$FQBN" --port "$PORT" "$SKETCH_DIR"; then
    echo "❌ Upload failed!"
    exit 1
fi

echo "=== Starting serial monitor ==="
echo "Port: $PORT"
echo "Baudrate: $BAUDRATE"
echo "Output log: $LOG_FILE"
if [ "$DURATION_SEC" -gt 0 ]; then
    echo "Duur: $DURATION_SEC sec"
else
    echo "Duur: onbeperkt (Ctrl+C om te stoppen)"
fi
echo ""
echo "Press Ctrl+C to stop monitoring"
echo "=================================="
echo ""

# Maak timestamp voor log (alleen tekst)
printf "=== Serial Monitor Started: %s ===\n" "$(date)" >> "$LOG_FILE"
printf "Port: %s, Baudrate: %s, Duur: %s sec\n\n" "$PORT" "$BAUDRATE" "$DURATION_SEC" >> "$LOG_FILE"

# Start serial monitor en filter alleen leesbare tekst
# Gebruik LC_ALL=C om binary data te voorkomen en UTF-8 problemen te vermijden
export LC_ALL=C
start_monitor() {
    arduino-cli monitor \
        --port "$PORT" \
        --config baudrate="$BAUDRATE" \
        2>&1 | \
        while IFS= read -r line || [ -n "$line" ]; do
        # Filter alleen printable ASCII characters (0x20-0x7E) en newlines/tabs
        filtered_line=$(echo "$line" | tr -cd '\11\12\15\40-\176' 2>/dev/null || echo "$line" | sed 's/[^[:print:][:space:]]//g' 2>/dev/null || echo "$line")
        
        # Verwijder lege regels en alleen whitespace
        trimmed_line=$(echo "$filtered_line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//' 2>/dev/null || echo "$filtered_line")
        
        if [ -n "$trimmed_line" ]; then
            # Print naar terminal
            echo "$trimmed_line"
            # Schrijf naar log file (alleen leesbare tekst)
            echo "$trimmed_line" >> "$LOG_FILE"
        fi
    done
}

if [ "$DURATION_SEC" -gt 0 ]; then
    start_monitor &
    MON_PID=$!
    sleep "$DURATION_SEC"
    kill "$MON_PID" 2>/dev/null
    printf "=== Serial Monitor Stopped: %s ===\n" "$(date)" >> "$LOG_FILE"
else
    start_monitor
fi

