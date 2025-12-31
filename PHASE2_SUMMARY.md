# Phase 2 Implementation Summary

## What Was Completed

Phase 2 focuses on implementing CSI data streaming from the ESP32 to enable real-time analysis on a laptop.

### Firmware Changes

1. **CSI Data Streaming (wifi_csi.c:132-148)**
   - Modified `wifi_csi_rx_cb()` callback to output CSI data in JSON format
   - Each packet is printed to serial as a single JSON line
   - Format: `{"ts":12345,"rssi":-45,"num":64,"amp":[...],"phase":[...]}`
   - This allows easy parsing by laptop-side tools

2. **Main Application Update (main.c:237)**
   - Added logging message to indicate CSI streaming is active
   - No other changes needed - existing CSI collection works as-is

### Configuration & Documentation

3. **WiFi Credentials Setup**
   - Created `.env.example` as a template for WiFi configuration
   - Added `.gitignore` to prevent committing sensitive credentials
   - Note: Manual update still needed in `main.c` until build system integration

4. **Firmware README (firmware/README.md)**
   - Complete setup instructions for ESP-IDF
   - WiFi configuration guide
   - Build and flash commands
   - Serial output format documentation
   - Troubleshooting section

### Testing Tools

5. **CSI Data Reader (tools/read_csi.py)**
   - Python script to read and display CSI data from serial port
   - Features:
     - Real-time display of packet statistics
     - Optional verbose mode showing full CSI arrays
     - Save CSI data to JSONL file for later analysis
   - Usage: `python3 read_csi.py /dev/ttyUSB0 -v -o data.jsonl`

6. **Python Requirements (tools/requirements.txt)**
   - Added pyserial dependency for serial communication

## What Still Needs to Be Done

### Immediate Next Steps (Hardware Testing)

These tasks require physical ESP32-S3 hardware:

1. **Install ESP-IDF v5.x**
   ```bash
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh esp32s3
   source export.sh
   ```

2. **Configure WiFi Credentials**
   - Edit `firmware/main/main.c` lines 33-34:
   ```c
   #define WIFI_SSID      "YourNetworkName"
   #define WIFI_PASSWORD  "YourPassword"
   ```

3. **Build and Flash Firmware**
   ```bash
   cd firmware
   idf.py set-target esp32s3
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

4. **Test CSI Data Collection**
   - Verify WiFi connection succeeds
   - Confirm CSI packets appear in serial output
   - Use `tools/read_csi.py` to validate data format

5. **Validate CSI Data Quality**
   - Check that amplitude values are reasonable (not all zeros)
   - Verify phase values are in range [-π, π]
   - Confirm RSSI values make sense for the environment
   - Test with/without human presence to see if CSI changes

### Future Phase 2 Work

6. **Web-Based Visualization**
   - Create HTML/JS UI using WebSerial API
   - Real-time plotting of amplitude/phase data
   - Record and save datasets from browser
   - Compare CSI with/without human presence

7. **Build System Integration**
   - Update CMakeLists.txt to read WiFi credentials from `.env`
   - Auto-generate header file with credentials at build time
   - Remove hardcoded credentials from source files

## File Structure

```
DensePose-ESP32/
├── .env.example              # WiFi config template
├── .gitignore                # Git ignore rules
├── TODO.md                   # Updated project TODO list
├── PHASE2_SUMMARY.md         # This file
├── firmware/
│   ├── README.md             # Firmware setup guide
│   ├── CMakeLists.txt        # ESP-IDF project file
│   └── main/
│       ├── main.c            # Updated with streaming message
│       ├── wifi_csi.c        # Updated with JSON output
│       └── wifi_csi.h
└── tools/
    ├── read_csi.py           # Python CSI reader
    └── requirements.txt      # Python dependencies
```

## Testing Checklist

When you have the hardware, run through this checklist:

- [ ] ESP-IDF v5.x installed and working
- [ ] WiFi credentials configured in main.c
- [ ] Firmware builds without errors
- [ ] Firmware flashes successfully to ESP32-S3
- [ ] ESP32 connects to WiFi network
- [ ] CSI data appears in serial monitor
- [ ] JSON format is valid and parseable
- [ ] `read_csi.py` can read and display the data
- [ ] Amplitude values look reasonable (non-zero)
- [ ] Phase values are in expected range
- [ ] RSSI correlates with distance from router
- [ ] CSI data changes when person moves in environment

## Known Issues / Future Improvements

1. **WiFi credentials hardcoded**: Need to implement .env build integration
2. **No filtering**: Raw CSI data includes noise - Phase 3 will add filtering
3. **High data rate**: May want to add packet rate limiting for slower systems
4. **Fixed logging**: Some ESP-IDF logs mixed with JSON - could add mode switch

## Key Learnings

1. **CSI Callback Context**: The `wifi_csi_rx_cb()` runs in WiFi task context, so keep it fast
2. **JSON Format**: Simple newline-delimited JSON is easy to parse and debug
3. **Printf Performance**: Direct printf() is fast enough for CSI streaming at typical rates
4. **Serial Baud Rate**: 115200 baud is sufficient for 64 subcarriers at reasonable packet rates

## References

- ESP32 WiFi CSI API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wi-fi-channel-state-information
- WebSerial API: https://web.dev/serial/
- DensePose Paper: See docs/DensePose-WiFi-Paper.md
