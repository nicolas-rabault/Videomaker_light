#!/bin/bash

# This script allow to update the complete product.
#    - Step 1 compile all binaries for every boards
#    - Step 2 deploy all binaries on every boards
# For additional information, please refer to the README.md file.

# Compile all binaries.
pio run -d led_strip/ -e l0_with_bootloader
pio run -d potentiometer/ -e l0_with_bootloader

# Flash the led_strip
pyluos-bootloader flash -t 3 -b led_strip/.pio/build/l0_with_bootloader/firmware.bin

# Flash the detector steps (the one on the botom)
pyluos-bootloader flash -t 2 -b potentiometer/.pio/build/l0_with_bootloader/firmware.bin
