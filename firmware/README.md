# DensePose-ESP32 Firmware

This firmware collects WiFi Channel State Information (CSI) on the ESP32-S3 and streams it over serial for real-time analysis.

## Hardware Requirements

- ESP32-S3-Zero (Waveshare) or compatible ESP32-S3 board
- USB cable for programming and serial communication
- WiFi router/access point

## Software Requirements

- ESP-IDF v5.x (ESP32-S3 toolchain)
- Python 3.7+ (for ESP-IDF tools)

## Setup

### 1. Install ESP-IDF

```bash
# Clone ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Install for ESP32-S3
./install.sh esp32s3

# Source environment variables (run this in every new terminal)
source export.sh
```

### 2. Configure WiFi Credentials

Create a `.env` file in the project root:

```bash
cd /path/to/DensePose-ESP32
cp .env.example .env
```

Edit `.env` and set your WiFi credentials:

```
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=YourPassword
```

**Note**: For Phase 2 testing, you'll need to manually update the credentials in `firmware/main/main.c` lines 33-34 until the build system integration is complete.

### 3. Build and Flash

```bash
cd firmware

# Set target to ESP32-S3
idf.py set-target esp32s3

# Build the firmware
idf.py build

# Flash to the board (replace /dev/ttyUSB0 with your port)
# On macOS: /dev/cu.usbserial-*
# On Windows: COM3, COM4, etc.
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor
```

To exit the monitor, press `Ctrl+]`.

### 4. Flash and Monitor (Combined)

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Serial Output Format

The firmware streams CSI data in JSON format over serial:

```json
{"ts":12345,"rssi":-45,"num":64,"amp":[1.2,3.4,...],"phase":[0.123,-1.234,...]}
```

Fields:
- `ts`: Timestamp in milliseconds
- `rssi`: Received Signal Strength Indicator (dBm)
- `num`: Number of subcarriers (typically 64)
- `amp`: Array of amplitude values (signal strength per subcarrier)
- `phase`: Array of phase values in radians (signal timing per subcarrier)

## Troubleshooting

### ESP-IDF not found

Make sure you've sourced the export script:
```bash
source ~/esp/esp-idf/export.sh
```

### Permission denied on serial port

On Linux:
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### WiFi connection fails

1. Check WiFi credentials in `main.c`
2. Ensure your router is on 2.4GHz (ESP32 doesn't support 5GHz)
3. Check serial output for error messages

### No CSI data appearing

1. Verify WiFi connection is successful
2. Check that there's WiFi traffic on the network
3. Try pinging the ESP32 from another device to generate packets

## Next Steps (Phase 2)

- [ ] Test CSI data collection on hardware
- [ ] Verify CSI packets are being received
- [ ] Validate CSI data quality
- [ ] Create web-based visualization tool using WebSerial API

## Reference

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [ESP32 WiFi CSI Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wi-fi-channel-state-information)
