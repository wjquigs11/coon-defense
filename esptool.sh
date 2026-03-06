#!/bin/bash

# Default port
port="/dev/cu.usbserial-0001"
run_miniterm=true

# Parse arguments
i=1
while [[ $i -le $# ]]; do
    arg="${!i}"
    if [[ "$arg" == "-m" ]]; then
        run_miniterm=true
    elif [[ "$arg" == "-p" ]]; then
        # Get the next argument as the port value
        ((i++))
        if [[ $i -le $# ]]; then
            port="${!i}"
        else
            echo "Error: -p requires a port argument"
            exit 1
        fi
    fi
    ((i++))
done

esptool.py --chip esp32 --port $port --baud 460800 write_flash 0x10000 .pio/build/esp32dev/firmware.bin

if [[ "$run_miniterm" == true ]]; then
    /usr/bin/python3 -m serial.tools.miniterm $port 115200
fi
