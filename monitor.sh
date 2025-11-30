#!/bin/bash
# Serial monitor script voor UNIFIED-LVGL9-Crypto_Monitor
# Gebruik: ./monitor.sh [port] [baudrate]
# Bijvoorbeeld: ./monitor.sh /dev/cu.usbserial-1410 115200
# Output wordt opgeslagen in serial_output.log (alleen leesbare tekst)

SKETCH_DIR="/Users/janpieterduhen/MEGA/@HOKUSAI/Arduino_nieuw/UNIFIED-LVGL9-Crypto_Monitor"
LOG_FILE="$SKETCH_DIR/serial_output.log"
BAUDRATE="${2:-115200}"  # Default 115200 als niet opgegeven

cd "$SKETCH_DIR"

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

echo "=== Starting serial monitor ==="
echo "Port: $PORT"
echo "Baudrate: $BAUDRATE"
echo "Output log: $LOG_FILE"
echo ""
echo "Press Ctrl+C to stop monitoring"
echo "=================================="
echo ""

# Maak timestamp voor log (alleen tekst)
printf "=== Serial Monitor Started: %s ===\n" "$(date)" >> "$LOG_FILE"
printf "Port: %s, Baudrate: %s\n\n" "$PORT" "$BAUDRATE" >> "$LOG_FILE"

# Start serial monitor en filter alleen leesbare tekst
# Gebruik LC_ALL=C om binary data te voorkomen en UTF-8 problemen te vermijden
export LC_ALL=C
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

