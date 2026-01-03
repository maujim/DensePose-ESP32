# DensePose-ESP32 Deployment Guide

This guide covers deploying the DensePose-ESP32 system in production environments, from initial setup to ongoing monitoring.

## Table of Contents

1. [Hardware Setup](#hardware-setup)
2. [Firmware Deployment](#firmware-deployment)
3. [Network Configuration](#network-configuration)
4. [Calibration](#calibration)
5. [Monitoring](#monitoring)
6. [Troubleshooting](#troubleshooting)
7. [Maintenance](#maintenance)

## Hardware Setup

### Requirements

- ESP32-S3 board with PSRAM (tested: Waveshare ESP32-S3-Zero)
- USB power supply (5V, 1A minimum)
- WiFi access point (2.4GHz only)
- Optional: Serial monitor for debugging

### Physical Placement

For optimal CSI signal quality:

1. **Distance**: 1-5 meters from target area
2. **Height**: 1-2 meters above ground
3. **Orientation**: Antenna facing target area
4. **Obstructions**: Minimize between ESP32 and WiFi router
5. **Metal objects**: Keep away from metal surfaces

### Antenna Considerations

The ESP32-S3-Zero has an onboard PCB antenna. For better range:

- Use external antenna if available
- Position for clear line-of-sight to router
- Avoid enclosed metal enclosures

## Firmware Deployment

### 1. Configure WiFi

```bash
# Create environment file
cp .env.example .env

# Edit with your credentials
nano .env
```

Add your WiFi credentials:
```
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=YourPassword
```

### 2. Build Firmware

```bash
cd firmware

# Set target
idf.py set-target esp32s3

# Configure (optional)
idf.py menuconfig

# Build
idf.py build
```

**Important menuconfig settings:**
- Component config → ESP32S3-Specific → SPIRAM: **Enable**
- Component config → WiFi → WiFi CSI: **Enable**
- Component config → WiFi → CSI config:
  - LLTF: **Enable**
  - HT-LTF: **Enable**
  - Channel Filter: **Enable**

### 3. Flash to Device

```bash
# Find serial port
ls /dev/ttyUSB*  # Linux
ls /dev/tty.usb* # macOS

# Flash firmware
idf.py -p /dev/ttyUSB0 flash

# Monitor output (optional)
idf.py -p /dev/ttyUSB0 monitor
```

Press `Ctrl+]` to exit monitor.

### 4. Verify Deployment

Check serial output for:
```
I (1234) main: DensePose-ESP32 starting...
I (1567) wifi_csi: CSI collection initialized successfully
I (1901) pose_inference: Pose estimation initialized successfully
I (2345) main: Initialization complete. Collecting CSI data...
```

## Network Configuration

### WiFi Requirements

- **Frequency**: 2.4GHz only (ESP32 doesn't support 5GHz)
- **Security**: WPA2-PSK or WPA3
- **Channel**: Any (auto-select recommended)
- **Bandwidth**: 20MHz recommended (better CSI)

### Router Settings

For optimal CSI collection:

1. **Enable WiFi**: Keep router transmitting
2. **Minimum traffic**: One device connected generates sufficient traffic
3. **Channel width**: Set to 20MHz if configurable
4. **TX power**: Medium to high

### Static IP (Optional)

For production deployments, configure static IP:

```bash
idf.py menuconfig
# Component config → LWIP → DHCP
# Disable DHCP, set static IP
```

## Calibration

Before using the system in production, perform calibration:

### 1. Baseline Collection (Empty Room)

Run the system with no people present for 2-3 minutes:

```bash
cd tools/visualizer
python3 -m http.server 8000
# Open browser, connect serial, record data
```

Note the typical:
- Amplitude mean: ~15-25
- Amplitude std: < 3.0
- Phase variance: < 0.1

### 2. Presence Calibration

Have a person sit in the detection zone for 2-3 minutes:

Note the changes:
- Amplitude std: 5-8
- Phase variance: 0.3-0.5

### 3. Adjust Thresholds

Edit `firmware/main/pose_inference.c` if needed:

```c
// Line 174-177 in detect_human_presence()
const float AMP_STD_THRESHOLD_EMPTY = 2.0f;  // Adjust based on baseline
const float AMP_STD_THRESHOLD_PRESENT = 5.0f; // Adjust based on presence test
const float PHASE_VAR_THRESHOLD_MOVING = 0.5f;
```

### 4. Test All Poses

Verify detection for each pose class:
- Empty room → "Empty"
- Person sitting still → "Sitting" or "Present"
- Person standing still → "Standing" or "Present"
- Person walking → "Walking" or "Moving"
- Person moving around → "Moving"

### 5. Recalibrate If Needed

Re-calibrate when:
- Moving to new environment
- Changing WiFi router
- Significant furniture changes
- Seasonal changes (affects RF propagation)

## Monitoring

### Web-Based Visualizer

```bash
cd tools/visualizer
python3 -m http.server 8000
```

Open browser to `http://localhost:8000`:
- Connect to serial port
- View real-time CSI data
- Monitor pose detection results
- Export data for analysis

### Serial Monitoring

For headless monitoring:

```bash
cd firmware
idf.py -p /dev/ttyUSB0 monitor | tee monitor.log
```

Key indicators:
- Packets received per second: 10-100 Hz
- RSSI: -30 to -70 dBm (stronger is better)
- Inference rate: ~2 Hz (every 500ms)

### Performance Metrics

Monitor these metrics:

| Metric | Healthy | Warning | Action |
|--------|---------|---------|--------|
| WiFi Connection | Connected | Disconnected | Check router/network |
| CSI Packets | >10 Hz | <1 Hz | Check CSI config |
| RSSI | -30 to -60 | <-70 dBm | Move closer to router |
| Free Heap | >200KB | <100KB | Check memory leaks |
| Inference Rate | ~2 Hz | <0.5 Hz | Check CPU load |

### Data Logging

Enable persistent logging:

```c
// In main.c, add:
esp_log_set_timestamp(esp_log_timestamp);

// Redirect to file:
esp_log_level_set("*", ESP_LOG_INFO);
```

## Troubleshooting

### WiFi Won't Connect

**Symptoms**: "Failed to connect" in serial output

**Solutions**:
1. Verify SSID/password in `.env`
2. Check 2.4GHz network (not 5GHz)
3. Confirm router is broadcasting
4. Try reducing distance to router
5. Check for MAC filtering

### No CSI Data

**Symptoms**: Connected but no CSI packets

**Solutions**:
1. Verify CSI enabled in menuconfig
2. Check router is transmitting (need traffic)
3. Try different WiFi channel
4. Verify CSI initialization in logs
5. Reboot router and ESP32

### Poor Detection Accuracy

**Symptoms**: Wrong pose classifications

**Solutions**:
1. Recalibrate thresholds (see Calibration section)
2. Check environmental changes
3. Verify target is in detection zone
4. Collect more training data
5. Train custom ML model

### High Memory Usage

**Symptoms**: "Out of memory" errors

**Solutions**:
1. Check PSRAM is enabled
2. Reduce buffer sizes in `pose_inference.c`
3. Reduce logging verbosity
4. Check for memory leaks (monitor free heap)

### System Freezes

**Symptoms**: No output, unresponsive

**Solutions**:
1. Check watchdog timer issues
2. Increase task stack sizes
3. Add `vTaskDelay()` in long operations
4. Enable panic handler for debugging

## Maintenance

### Regular Tasks

**Weekly**:
- Check free heap size in logs
- Verify WiFi connection stability
- Review detection accuracy

**Monthly**:
- Recalibrate if needed
- Export and analyze data
- Check for firmware updates

**Quarterly**:
- Collect new training data
- Retrain ML models
- Review and update thresholds

### Firmware Updates

```bash
# Pull latest code
git pull

# Rebuild
cd firmware
idf.py build

# Reflash
idf.py -p /dev/ttyUSB0 flash
```

### Data Management

**Export accumulated data**:
```bash
# From visualizer, click "Export Data"
# Or from serial log:
idf.py -p /dev/ttyUSB0 monitor | tee backup_$(date +%Y%m%d).log
```

**Archive old datasets**:
```bash
mkdir -p archives/$(date +%Y%m)
mv datasets/old_* archives/$(date +%Y%m)/
```

### Backup Configuration

Keep backup of:
- `.env` file (WiFi credentials)
- `sdkconfig` (firmware configuration)
- Calibration thresholds (document values)

## Security Considerations

### WiFi Credentials

- Never commit `.env` to version control
- Use strong WiFi passwords
- Consider guest network for ESP32

### Serial Access

- In production, disable serial monitoring
- Use secure boot if available
- Limit physical access to device

### Data Privacy

CSI data can reveal:
- Human presence/absence
- Movement patterns
- Activity levels

Consider:
- Encrypting serial output
- Not storing raw CSI data long-term
- Anonymizing data before analysis

## Performance Optimization

### For Better Accuracy

1. **Optimal placement**: 2-3m from target, clear line-of-sight
2. **Reduce interference**: Away from microwaves, other WiFi
3. **Use ML model**: Train on environment-specific data
4. **Multi-board setup**: Use multiple ESP32s for spatial diversity

### For Lower Power

1. **Reduce sampling rate**: 50Hz instead of 100Hz
2. **Sleep between windows**: Power save mode
3. **Lower logging**: Reduce ESP_LOG level
4. **Optimize model**: Use INT8 quantization

### For Lower Latency

1. **Smaller windows**: 250ms instead of 500ms
2. **Optimized model**: Reduce layers/filters
3. **Faster CPU**: 240MHz max (already default)
4. **PSRAM for buffers**: Offload from internal RAM

## Production Checklist

Before deploying to production:

- [ ] WiFi credentials configured
- [ ] PSRAM enabled in menuconfig
- [ ] CSI enabled and tested
- [ ] Calibration performed
- [ ] All pose classes tested
- [ ] Visualizer connecting properly
- [ ] Data export working
- [ ] Serial monitoring configured
- [ ] Power supply stable
- [ ] Physical placement optimized
- [ ] Documentation complete
- [ ] Backup procedure tested
- [ ] Maintenance schedule defined

## Support and Resources

- **Documentation**: See `docs/` directory
- **Issues**: Check GitHub issues
- **Paper**: [DensePose From WiFi](https://arxiv.org/abs/2301.00250)
- **ESP-IDF**: [Espressif Documentation](https://docs.espressif.com/projects/esp-idf/)

---

**Last Updated**: 2026-01-03
**Version**: 1.0
