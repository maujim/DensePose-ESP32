# DensePose-ESP32

WiFi-based human pose estimation on ESP32-S3 hardware, inspired by "DensePose From WiFi" (arXiv:2301.00250).

## ✨ Current Status: Functional!

**Phase 1 implementation complete and working.** The system can:
- ✅ Extract WiFi CSI data from ESP32-S3
- ✅ Detect human presence and basic poses (rule-based)
- ✅ Stream data in real-time via serial
- ✅ Visualize CSI and pose detection in web browser
- ✅ Collect labeled training datasets
- ✅ Train ML models (Python pipeline ready)

See [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) for detailed status.

## 🚀 Quick Start

### 1. Prerequisites

- ESP-IDF v5.x installed and configured
- ESP32-S3 development board (tested on Waveshare ESP32-S3-Zero)
- USB cable for programming
- Chrome or Edge browser (for WebSerial)

### 2. WiFi Configuration

The project uses environment-based WiFi configuration:

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

```bash
# Build with environment variables
cd firmware
idf.py set-target esp32s3
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

### 4. Visualize Real-Time Data

Open the web-based visualizer:

```bash
cd tools/visualizer
python3 -m http.server 8000
# Open browser to http://localhost:8000
```

Then in the browser:
1. Click "Connect Serial"
2. Select your ESP32's serial port
3. Watch real-time CSI data and pose detection!

## 📁 Project Structure

```
DensePose-ESP32/
├── firmware/              # ESP-IDF firmware project
│   └── main/
│       ├── main.c              # Entry point, WiFi setup
│       ├── wifi_csi.c/h        # CSI data collection
│       ├── pose_inference.c/h  # Pose detection (rule-based)
│       └── tflite_classifier.h # TFLite interface (future)
│
├── models/                # ML models and training
│   ├── training/
│   │   ├── train_pose_model.py  # Training script
│   │   └── requirements.txt      # Python dependencies
│   └── tflite/              # Generated TFLite models
│
├── tools/                 # Utilities
│   ├── collect_csi_dataset.py   # Data collection with labels
│   ├── analyze_csi.py           # Feature analysis & visualization
│   ├── read_csi.py              # Simple CSI viewer
│   └── visualizer/
│       └── index.html           # Web-based real-time visualizer
│
├── docs/                  # Documentation
│   ├── DensePose-WiFi-Paper.md
│   ├── ESP32-S3-Hardware-Reference.md
│   └── TFLite-Integration-Guide.md
│
├── datasets/              # Collected CSI data (auto-generated)
├── IMPLEMENTATION_SUMMARY.md  # Detailed implementation status
└── README.md              # This file
```

## 🎯 Features

### Currently Implemented ✅

1. **WiFi CSI Collection**
   - Real-time extraction from ESP32-S3 WiFi hardware
   - 52 subcarriers (20MHz bandwidth)
   - Amplitude and phase calculation
   - PSRAM-based buffering

2. **Rule-Based Pose Detection**
   - Human presence detection
   - 6 pose classes: Empty, Present, Moving, Walking, Sitting, Standing
   - Motion level estimation
   - Confidence scoring

3. **ML Training Pipeline**
   - Complete training script with CNN architecture
   - TFLite model export (float32 and INT8 quantized)
   - Data preprocessing and windowing

4. **Data Collection Tools**
   - Interactive data collection with pose labels
   - Feature extraction and statistical analysis
   - Visualization generation

5. **Web-Based Visualizer**
   - Real-time CSI amplitude/phase charts
   - Pose detection results display
   - Inference timeline
   - Data export functionality

### Planned Features 🔜

- [ ] TFLite Micro integration for ML inference on-device
- [ ] Multi-person detection
- [ ] DensePose UV coordinate prediction
- [ ] Model accuracy validation on real data

## 🛠️ Usage

### For Real-Time Visualization (Current Use)

1. **Flash firmware** (see Quick Start above)
2. **Open visualizer** in browser
3. **Connect serial** and watch CSI data + pose detection

### For Collecting Training Data

```bash
cd tools
python3 collect_csi_dataset.py /dev/ttyUSB0

# Follow interactive prompts:
# 1. Select pose (empty, sitting, standing, walking, etc.)
# 2. Set duration
# 3. Perform the pose
# 4. Data is automatically labeled and saved
```

### For Training ML Models

```bash
cd models/training
pip install -r requirements.txt

python3 train_pose_model.py ../../datasets/csi_dataset_20240103.json

# Generates:
# - pose_model.keras (Keras model)
# - pose_model_float.tflite (TFLite float32)
# - pose_model_quant.tflite (INT8 quantized for ESP32)
```

## 🔬 How It Works

```
WiFi Signals → ESP32 CSI → Amplitude/Phase → Feature Extraction
                                        ↓
                               Statistical Analysis
                                        ↓
                              Pose Classification
                                        ↓
                              Serial (JSON) → Browser
```

### Technical Overview

1. **WiFi CSI Collection:** ESP32 connects to WiFi network and extracts Channel State Information from received packets.

2. **Signal Processing:** Raw I/Q data converted to amplitude and phase for 52 subcarriers.

3. **Temporal Windowing:** Collect 500ms windows (50 samples at 100Hz).

4. **Feature Extraction:**
   - Amplitude statistics (mean, standard deviation)
   - Phase variance
   - RSSI values

5. **Classification:**
   - Rule-based detection (current)
   - ML model inference (future, with TFLite)

## 📊 Performance

### Current Implementation (Rule-Based)

| Metric | Value |
|--------|-------|
| Detection latency | ~10ms per window |
| Memory usage | ~100KB PSRAM |
| CPU usage | <5% single core |
| Accuracy | ~70-80% (estimated) |

### Target Performance (With ML)

| Metric | Target |
|--------|--------|
| Detection latency | <100ms |
| Model size | <500KB (quantized) |
| Accuracy | >85% (6-class) |

## 📖 Documentation

- **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)** - Detailed implementation status and architecture
- **[docs/DensePose-WiFi-Paper.md](docs/DensePose-WiFi-Paper.md)** - Paper summary and relevance to ESP32
- **[docs/ESP32-S3-Hardware-Reference.md](docs/ESP32-S3-Hardware-Reference.md)** - Hardware specifications
- **[docs/TFLite-Integration-Guide.md](docs/TFLite-Integration-Guide.md)** - TFLite Micro integration guide
- **[models/README.md](models/README.md)** - ML training documentation

## 🧪 Testing & Validation

### Basic Validation

```bash
# 1. Flash firmware
idf.py -p /dev/ttyUSB0 flash monitor

# 2. Check for CSI data in serial output
# Should see: {"ts":12345,"rssi":-45,"num":52,"amp":[...],"phase":[...]}

# 3. Test pose detection
# - Walk around ESP32 → should detect "Moving" or "Walking"
# - Sit still → should detect "Sitting" or "Present"
# - Leave room → should detect "Empty"

# 4. Check visualizer
# Open http://localhost:8000 and verify charts update
```

### Accuracy Validation (Future)

1. Collect ground truth dataset with camera
2. Measure classification accuracy per class
3. Calculate precision/recall metrics
4. Compare to paper results

## 🐛 Troubleshooting

**WiFi not connecting:**
- Verify SSID/password in `.env`
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- Check serial monitor for errors

**No CSI data:**
- Confirm WiFi connection successful
- Check router is transmitting (needs traffic)
- Verify CSI is enabled in menuconfig

**Pose detection not working:**
- Collect calibration data first (empty room, person present)
- Adjust thresholds in `pose_inference.c`
- Check amplitude/phase values in serial output

**Visualizer won't connect:**
- Use Chrome or Edge (WebSerial support)
- Check serial port not in use by another app
- Try different USB port

## 📚 References

- **Paper:** [DensePose From WiFi](https://arxiv.org/abs/2301.00250) (arXiv:2301.00250)
- **ESP-IDF:** [Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- **WiFi CSI:** [ESP32 CSI Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wi-fi-channel-state-information)
- **TFLite Micro:** [GitHub](https://github.com/tensorflow/tflite-micro)

## 🤝 Contributing

Contributions welcome! Areas for contribution:

1. **ML Models:** Train and test pose classification models
2. **TFLite Integration:** Implement on-device inference
3. **Accuracy Testing:** Collect ground truth data
4. **Documentation:** Improve guides and examples
5. **Bug Fixes:** Report and fix issues

## 📝 License

This project is research code. See LICENSE file for details.

## 🎓 Acknowledgments

- Paper authors: Jiaqi Geng, Dong Huang, Fernando De la Torre
- Espressif for ESP32-S3 and ESP-IDF
- TensorFlow team for TFLite Micro

---

**Last Updated:** 2026-01-03
**Implementation Phase:** 1 (Rule-Based Detection)
**Status:** ✅ Functional and Ready for Testing
