#!/bin/bash
# Build script for DensePose-ESP32
# This script loads WiFi credentials from .env and builds the firmware

set -e  # Exit on error

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== DensePose-ESP32 Build Script ===${NC}"

# Check if .env exists
if [ ! -f .env ]; then
    echo -e "${YELLOW}Warning: .env file not found${NC}"
    echo -e "${YELLOW}Please copy .env.example to .env and fill in your WiFi credentials${NC}"
    echo ""
    echo "Example:"
    echo "  cp .env.example .env"
    echo "  nano .env"
    echo ""
    read -p "Continue with default values? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Load environment variables from .env
if [ -f .env ]; then
    echo -e "${GREEN}Loading WiFi credentials from .env...${NC}"
    # Read .env file line by line to properly handle special characters
    while IFS='=' read -r key value; do
        # Skip comments and empty lines
        if [[ ! $key =~ ^#.* ]] && [[ -n $key ]]; then
            # Remove quotes if present
            value="${value%\"}"
            value="${value#\"}"
            value="${value%\'}"
            value="${value#\'}"
            export "$key=$value"
        fi
    done < .env
    echo "WIFI_SSID: $WIFI_SSID"
    echo "WIFI_PASSWORD: ***"
fi

# Change to firmware directory
cd firmware

# Set target if not already set
if [ ! -f sdkconfig ]; then
    echo -e "${GREEN}Setting ESP-IDF target to esp32s3...${NC}"
    idf.py set-target esp32s3
fi

# Build the project
echo -e "${GREEN}Building firmware...${NC}"
idf.py build

echo -e "${GREEN}Build complete!${NC}"
echo ""
echo "To flash the firmware:"
echo "  cd firmware"
echo "  idf.py -p /dev/ttyUSB0 flash monitor"
