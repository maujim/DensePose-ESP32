# DensePose-ESP32

WiFi-based human pose estimation on ESP32-S3 hardware, inspired by "DensePose From WiFi" (arXiv:2301.00250).

## Quick Start

### 1. Prerequisites

- ESP-IDF v5.x installed and configured
- ESP32-S3 development board (tested on Waveshare ESP32-S3-Zero)
- USB cable for programming

### 2. WiFi Configuration

The project uses environment-based WiFi configuration for security and convenience.

```bash
# Copy the example environment file
cp .env.example .env

# Edit .env with your WiFi credentials
nano .env
```

Your `.env` file should look like:
```
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=YourNetworkPassword
```

**Important:** Never commit `.env` to version control! It's already in `.gitignore`.

### 3. Build and Flash

#### Option A: Using the build script (recommended)

```bash
# The build script automatically loads WiFi credentials from .env
./build.sh

# Flash to device
cd firmware
idf.py -p /dev/ttyUSB0 flash monitor
```

#### Option B: Manual build with environment variables

```bash
# Load environment variables
export $(grep -v '^#' .env | xargs)

# Build
cd firmware
idf.py set-target esp32s3
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

#### Option C: Configure via menuconfig

If you don't want to use `.env`, you can configure WiFi credentials through menuconfig:

```bash
cd firmware
idf.py menuconfig
# Navigate to: DensePose WiFi Configuration
# Set WIFI_SSID and WIFI_PASSWORD
idf.py build
```

### 4. Monitor Output

```bash
cd firmware
idf.py -p /dev/ttyUSB0 monitor
```

Press `Ctrl+]` to exit the monitor.

## Project Structure

```
DensePose-ESP32/
├── .env.example           # Template for WiFi credentials
├── .gitignore            # Excludes .env and build artifacts
├── build.sh              # Build script with env loading
├── firmware/             # ESP-IDF project
│   ├── CMakeLists.txt
│   ├── main/
│   │   ├── main.c        # Application entry point
│   │   ├── wifi_csi.c    # CSI data collection
│   │   ├── wifi_csi.h
│   │   └── Kconfig.projbuild  # WiFi configuration options
│   └── sdkconfig         # Generated build configuration
├── docs/                 # Documentation
└── tools/                # Helper scripts
    └── load_wifi_env.py  # Environment loader
```

## Hardware

- **Board:** Waveshare ESP32-S3-Zero
- **Chip:** ESP32-S3FH4R2
- **CPU:** Dual-core Xtensa LX7 @ 240MHz
- **Memory:** 512KB SRAM + 2MB PSRAM + 4MB Flash
- **Connectivity:** WiFi 802.11 b/g/n + Bluetooth 5.0 LE

See `docs/ESP32-S3-Hardware-Reference.md` for detailed specifications.

## How It Works

1. **WiFi CSI Collection:** The ESP32-S3 connects to your WiFi network and collects Channel State Information (CSI) from WiFi packets.

2. **Signal Processing:** CSI data contains amplitude and phase information for each WiFi subcarrier. Human bodies affect these signals through absorption and reflection.

3. **Pose Estimation:** (Future work) Process CSI patterns to estimate human pose without cameras, preserving privacy.

## Development

### Building with Custom Configuration

The WiFi configuration system works through ESP-IDF's Kconfig:

1. Environment variables (`WIFI_SSID`, `WIFI_PASSWORD`) from `.env`
2. Override defaults in `firmware/main/Kconfig.projbuild`
3. Compiled into firmware as `CONFIG_WIFI_SSID` and `CONFIG_WIFI_PASSWORD`

### Changing WiFi Settings

To connect to a different network:

```bash
# Edit .env
nano .env

# Rebuild
./build.sh

# Reflash
cd firmware
idf.py -p /dev/ttyUSB0 flash
```

### Troubleshooting

**WiFi not connecting:**
- Check SSID and password in `.env`
- Verify your router is on 2.4GHz (ESP32 doesn't support 5GHz)
- Check serial monitor output for error messages

**Build fails with missing WiFi config:**
- Ensure `.env` exists or use `idf.py menuconfig` to set credentials
- Verify `.env` format matches `.env.example`

**USB device not found:**
- Try `/dev/ttyACM0` instead of `/dev/ttyUSB0`
- Check USB cable supports data (not just power)
- Hold BOOT button while plugging in (on ESP32-S3-Zero)

## References

- **Paper:** [DensePose From WiFi](https://arxiv.org/abs/2301.00250)
- **ESP-IDF:** [Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- **WiFi CSI:** [ESP32 CSI Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wi-fi-channel-state-information)

## License

See LICENSE file for details.
