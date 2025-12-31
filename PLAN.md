# DensePose-ESP32 Enhancement Plan

## Goal
Add visual feedback and web-based visualization to make CSI data collection more useful.

## Changes

### 1. RGB LED Feedback (`led_status.c`)
- Drive the WS2812 RGB LED on GPIO21
- Blink pattern based on CSI activity:
  - **No WiFi**: Red slow blink
  - **WiFi connected, no CSI**: Blue slow pulse
  - **CSI receiving**: Green blink (rate proportional to packet rate)

### 2. HTTP Web Server (`http_server.c`)
- ESP-IDF httpd server on port 80
- Routes:
  - `/` - Main HTML page with real-time visualization
  - `/csi` - SSE endpoint streaming JSON CSI data
  - `/stats` - JSON with system stats

### 3. HTML/JS Visualization (embedded in flash)
- Real-time scrolling graph of CSI amplitudes
- Packet rate display
- Signal strength gauge
- No external dependencies

## New Files
```
firmware/main/
├── led_status.c/h      # WS2812 RGB LED control
├── http_server.c/h     # HTTP server with SSE
└── html_data.h         # Embedded HTML/JS (generated)
```

## Modified Files
```
firmware/main/
├── main.c              # Initialize LED and HTTP server
└── CMakeLists.txt      # Add new source files
```

## Technical Details

### WS2812 (NeoPixel) Driver
- Use RMT (Remote Control) peripheral for precise timing
- One GPIO (GPIO21) controls the LED
- 24-bit color: GRB format

### Server-Sent Events (SSE)
- Simple one-way streaming from ESP32 to browser
- `text/event-stream` content type
- `data: <json>\n\n` format

### CSI Data Flow
```
WiFi Hardware → CSI Callback → Queue → HTTP Server → Browser
                                    ↓
                               LED Status
```

## Memory Considerations
- HTML page ~5-10KB (stored in flash as string literal)
- HTTP server task: 4KB stack
- LED task: 2KB stack
- CSI queue: 10 entries x 264 bytes = ~2.6KB
