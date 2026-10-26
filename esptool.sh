#!/bin/bash

# Default port, build, and partition
port="/dev/cu.usbserial-0001"
build="ec2"
partition="app0"
run_miniterm=true

# Display help message
function show_help {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -p PORT     Specify the port (default: /dev/cu.usbserial-0001)"
    echo "  -b BUILD    Specify the build name (default: main)"
    echo "  -t TYPE     Specify partition type to flash (app0, app1, spiffs, littlefs, partitions, all) (default: app0)"
    echo "  -m          Run miniterm after flashing (default: true)"
    echo "  -h          Show this help message"
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h)
            show_help
            ;;
        -m)
            run_miniterm=true
            shift
            ;;
        -p)
            if [[ -n $2 ]]; then
                port="$2"
                shift 2
            else
                echo "Error: -p requires a port argument"
                exit 1
            fi
            ;;
        -b)
            if [[ -n $2 ]]; then
                build="$2"
                shift 2
            else
                echo "Error: -b requires a build argument"
                exit 1
            fi
            ;;
        -h)
            show_help
            ;;
        -t)
            if [[ -n $2 ]]; then
                partition="$2"
                shift 2
            else
                echo "Error: -t requires a partition argument (app0, app1, spiffs)"
                exit 1
            fi
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Function to get partition file from platformio.ini
get_partition_file() {
    local partition_file=""
    if [ -f "platformio.ini" ]; then
        # Look for board_build.partitions setting (uncommented)
        partition_file=$(grep -E "^[[:space:]]*board_build\.partitions[[:space:]]*=" platformio.ini | head -1 | cut -d'=' -f2 | xargs)
    fi
    echo "$partition_file"
}

# Get workspace_dir from platformio.ini if set, otherwise use .pio
get_build_root() {
    local ws_dir=""
    if [ -f "platformio.ini" ]; then
        ws_dir=$(grep -E "^[[:space:]]*workspace_dir[[:space:]]*=" platformio.ini | head -1 | cut -d'=' -f2 | xargs)
    fi
    if [ -n "$ws_dir" ]; then
        echo "${ws_dir}/build"
    else
        echo ".pio/build"
    fi
}

# Default ESP32 partition offsets (used when no custom partition file is specified)
DEFAULT_APP0_OFFSET="0x10000"
DEFAULT_SPIFFS_OFFSET="0x290000"

# Get partition file from platformio.ini
partition_file=$(get_partition_file)
build_root=$(get_build_root)
echo "Build root: $build_root"
use_default_layout=false

# Initialize partition offsets
app0_offset=""
app1_offset=""
spiffs_offset=""

if [ -n "$partition_file" ] && [ -f "$partition_file" ]; then
    echo "Using custom partition file: $partition_file"
    # Read partition offsets from custom CSV file
    while IFS=, read -r name type subtype offset size flags; do
        # Skip comments and empty lines
        [[ $name =~ ^#.*$ || -z $name ]] && continue
        
        # Remove whitespace
        name=$(echo $name | xargs)
        offset=$(echo $offset | xargs)
        
        # Store offsets in separate variables
        if [ "$name" = "app0" ]; then
            app0_offset=$offset
        elif [ "$name" = "app1" ]; then
            app1_offset=$offset
        elif [ "$name" = "spiffs" ] || [ "$name" = "littlefs" ]; then
            spiffs_offset=$offset
        fi
    done < "$partition_file"
else
    if [ -n "$partition_file" ]; then
        echo "Warning: Partition file '$partition_file' specified in platformio.ini but not found."
    fi
    echo "Using default ESP32 partition layout"
    use_default_layout=true
    app0_offset=$DEFAULT_APP0_OFFSET
    spiffs_offset=$DEFAULT_SPIFFS_OFFSET
    # app1 is not available in default layout
fi

# Flash based on selected partition
case $partition in
    app0)
        if [ -z "$app0_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: app0 partition not available in default layout"
            else
                echo "Error: app0 partition not found in partition file"
            fi
            exit 1
        fi
        echo "Flashing firmware to app0 partition at $app0_offset"
        ~/.platformio/penv/bin/pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --chip esp32 --port $port --baud 460800 write_flash $app0_offset $build_root/$build/firmware.bin
        ;;
    app1)
        if [ "$use_default_layout" = true ]; then
            echo "Error: app1 partition is not available in default ESP32 layout"
            echo "To use app1, specify a custom partition file with OTA support in platformio.ini:"
            echo "  board_build.partitions = your_ota_partition_file.csv"
            exit 1
        fi
        if [ -z "$app1_offset" ]; then
            echo "Error: app1 partition not found in partition file"
            exit 1
        fi
        echo "Flashing firmware to app1 partition at $app1_offset"
        ~/.platformio/penv/bin/pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --chip esp32 --port $port --baud 460800 write_flash $app1_offset $build_root/$build/firmware.bin
        ;;
    spiffs|littlefs)
        if [ -z "$spiffs_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: spiffs/littlefs partition not available in default layout"
            else
                echo "Error: spiffs/littlefs partition not found in partition file"
            fi
            exit 1
        fi
        echo "Flashing filesystem data to partition at $spiffs_offset"
        # Add debug info about the filesystem image
        echo "Filesystem image details:"
        ls -la $build_root/$build/littlefs.bin 2>/dev/null || ls -la $build_root/$build/spiffs.bin 2>/dev/null
        
        # Determine the correct image file
        if [ -f "$build_root/$build/littlefs.bin" ]; then
            fs_image="$build_root/$build/littlefs.bin"
        elif [ -f "$build_root/$build/spiffs.bin" ]; then
            fs_image="$build_root/$build/spiffs.bin"
        else
            echo "Error: No filesystem image found (looked for littlefs.bin and spiffs.bin)"
            exit 1
        fi
        
        # Use explicit flash_mode and flash_freq parameters to ensure compatibility
        echo "Using explicit flash parameters for cross-platform compatibility"
        ~/.platformio/penv/bin/pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --chip esp32 --port $port --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect $spiffs_offset "$fs_image"
        ;;
    partitions)
        echo "Flashing partition table at 0x8000"
        ~/.platformio/penv/bin/pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --chip esp32 --port $port --baud 460800 write_flash 0x8000 $build_root/$build/partitions.bin
        ;;
    all)
        # Flash partition table first
        echo "Flashing partition table at 0x8000"
        ~/.platformio/penv/bin/pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --chip esp32 --port $port --baud 460800 write_flash 0x8000 $build_root/$build/partitions.bin

        # Flash firmware to app0
        if [ -z "$app0_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: app0 partition not available in default layout"
            else
                echo "Error: app0 partition not found in partition file"
            fi
            exit 1
        fi
        echo "Flashing firmware to app0 partition at $app0_offset"
        ~/.platformio/penv/bin/pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --chip esp32 --port $port --baud 460800 write_flash $app0_offset $build_root/$build/firmware.bin

        # Flash filesystem
        if [ -z "$spiffs_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: spiffs/littlefs partition not available in default layout"
            else
                echo "Error: spiffs/littlefs partition not found in partition file"
            fi
            exit 1
        fi
        echo "Flashing filesystem data to partition at $spiffs_offset"
        if [ -f "$build_root/$build/littlefs.bin" ]; then
            fs_image="$build_root/$build/littlefs.bin"
        elif [ -f "$build_root/$build/spiffs.bin" ]; then
            fs_image="$build_root/$build/spiffs.bin"
        else
            echo "Error: No filesystem image found (looked for littlefs.bin and spiffs.bin)"
            exit 1
        fi
        echo "Filesystem image: $fs_image"
        ls -la "$fs_image"
        ~/.platformio/penv/bin/pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --chip esp32 --port $port --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect $spiffs_offset "$fs_image"
        ;;
    *)
        if [ -z "$app0_offset" ]; then
            if [ "$use_default_layout" = true ]; then
                echo "Error: app0 partition not available in default layout"
            else
                echo "Error: app0 partition not found in partition file"
            fi
            exit 1
        fi
        echo "Unknown partition: $partition. Using default app0 at $app0_offset"
        ~/.platformio/penv/bin/pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --chip esp32 --port $port --baud 460800 write_flash $app0_offset $build_root/$build/firmware.bin
        ;;
esac

if [[ "$run_miniterm" == true ]]; then
    /usr/bin/python3 -m serial.tools.miniterm $port 115200
fi
