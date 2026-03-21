#!/usr/bin/env bash
# Apply LVGL 8.3 patch to lvgl_esp32_drivers and prepare config. Run from project root.
# Patch only applies to the pinned commit (LVGL 8.3 compatible). See README.
set -e

REQUIRED_COMMIT=335d444e450c6669a9f8f91c350def5dfaf53788

if [ ! -f "components/lvgl_esp32_drivers/CMakeLists.txt" ]; then
    echo "Error: Submodule not updated. Run: git submodule update --init --recursive"
    exit 1
fi

cd components/lvgl_esp32_drivers
CURRENT=$(git rev-parse HEAD)
if [ "$CURRENT" != "$REQUIRED_COMMIT" ]; then
    echo "Error: lvgl_esp32_drivers must be on commit $REQUIRED_COMMIT for this patch."
    echo "  Run: cd components/lvgl_esp32_drivers && git checkout $REQUIRED_COMMIT"
    exit 1
fi
git apply -p1 ../../patches/lvgl_esp32_drivers_8-3.patch
echo "LVGL driver patch applied successfully."

cd ../..

if [ -f "sdkconfig" ]; then
    echo "sdkconfig already exists, leaving it unchanged."
elif [ -f "sdkconfig.defaults" ]; then
    cp sdkconfig.defaults sdkconfig
    echo "sdkconfig.defaults copied to sdkconfig."
else
    echo "Warning: sdkconfig.defaults not found; no sdkconfig created."
fi
