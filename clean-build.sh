#!/bin/bash
# Clean build script - wist build cache om platform_config.h wijzigingen te forceren
# Gebruik: ./clean-build.sh

SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Cleaning Build Cache ==="
echo "This will remove all build artifacts to force a clean rebuild"
echo "This ensures platform_config.h changes are picked up correctly"
echo ""

# Verwijder build directory
if [ -d "$SKETCH_DIR/build" ]; then
    echo "Removing build/ directory..."
    rm -rf "$SKETCH_DIR/build"
    echo "✅ Build directory removed"
else
    echo "ℹ️  No build directory found"
fi

# Verwijder eventuele .o en .d files in de root
echo "Cleaning object files..."
find "$SKETCH_DIR" -maxdepth 1 -name "*.o" -delete 2>/dev/null
find "$SKETCH_DIR" -maxdepth 1 -name "*.d" -delete 2>/dev/null
find "$SKETCH_DIR" -maxdepth 1 -name "*.elf" -delete 2>/dev/null
find "$SKETCH_DIR" -maxdepth 1 -name "*.bin" -delete 2>/dev/null

echo ""
echo "✅ Build cache cleaned!"
echo ""
echo "Next steps:"
echo "1. Verify platform_config.h has the correct platform defined"
echo "2. Run ./upload.sh to compile with fresh cache"


