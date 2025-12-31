# DensePose-ESP32 TODO

## Legend
- [ ] Not started
- [x] Completed
- [~] In progress

---

## Phase 1: Hardware & Development Setup
- [x] Document hardware specifications (ESP32-S3FH4R2)
- [x] Research DensePose WiFi paper
- [x] Set up project structure
- [x] Create development documentation (AGENTS.md)
- [ ] Set up environment-based WiFi configuration
  - [ ] Create .env file for WiFi credentials (SSID/password)
  - [ ] Add .env to .gitignore
  - [ ] Update build system to read from .env and inject into firmware
- [ ] Install ESP-IDF v5.x
- [ ] Verify hardware connection and flashing
  - [ ] Install USB drivers for ESP32-S3-Zero
  - [ ] Test basic firmware upload (blink example)
  - [ ] Verify serial monitor output

## Phase 2: WiFi CSI Collection
- [x] Implement WiFi CSI module (wifi_csi.c/h)
  - [x] CSI callback infrastructure
  - [x] I/Q to amplitude/phase conversion
  - [x] Thread-safe data access
- [ ] Test CSI data collection on hardware
  - [ ] Build and flash firmware
  - [ ] Verify CSI packets are being received
  - [ ] Validate CSI data quality
- [ ] Web-based CSI visualization and analysis (laptop-side processing)
  - [ ] Update ESP32 firmware to stream CSI data over serial
  - [ ] Create HTML/JS web UI using WebSerial API
  - [ ] Real-time CSI data plotting (amplitude/phase visualization)
  - [ ] Record and save CSI datasets from browser
  - [ ] Test with/without human presence

## Phase 3: Signal Processing & Feature Extraction (Laptop-Side)
- [ ] Implement noise filtering in web UI
  - [ ] Remove DC offset (JavaScript)
  - [ ] Apply moving average filter
  - [ ] Handle outliers/bad packets
- [ ] Feature extraction from CSI (JavaScript/Python)
  - [ ] Time-domain features (variance, mean)
  - [ ] Frequency analysis (FFT if needed)
  - [ ] Subcarrier selection (identify most sensitive)
- [ ] Data collection for training
  - [ ] Set up controlled environment
  - [ ] Collect CSI samples via web UI (empty room baseline)
  - [ ] Collect CSI with human present
  - [ ] Label different poses/activities in browser
  - [ ] Export labeled dataset for training

## Phase 4: Human Presence Detection (Simplified ML)
- [ ] Laptop-side inference (initial implementation)
  - [ ] Prepare dataset (CSI → binary label: present/absent)
  - [ ] Train simple classifier (Random Forest, SVM, or small NN)
  - [ ] Evaluate accuracy
  - [ ] Run inference in browser (TensorFlow.js) or Python backend
  - [ ] Display real-time presence detection in web UI
- [ ] ESP32 deployment (optimization phase)
  - [ ] Convert model to TFLite
  - [ ] Quantize to INT8
  - [ ] Integrate TFLite Micro into firmware
  - [ ] Load model from flash
  - [ ] Run on-device inference
  - [ ] Test latency and memory usage
  - [ ] Compare on-device vs laptop performance

## Phase 5: Pose Estimation (Full Implementation)
- [ ] Laptop-side pose estimation (initial implementation)
  - [ ] Review DensePose paper network design
  - [ ] Set up camera + CSI synchronized recording via web UI
  - [ ] Collect paired CSI + pose labels
  - [ ] Generate DensePose annotations from images
  - [ ] Train pose estimation model on laptop
  - [ ] Run inference in browser (TensorFlow.js) or Python backend
  - [ ] Display real-time pose visualization in web UI
- [ ] ESP32 deployment (if feasible)
  - [ ] Design ESP32-compatible simplified version
  - [ ] Determine feasibility (memory/compute constraints)
  - [ ] Knowledge distillation for size reduction
  - [ ] Quantization and optimization
  - [ ] Flash model to ESP32
  - [ ] On-device pose inference
  - [ ] Benchmark and compare performance

## Phase 6: Optimization & Refinement
- [ ] Memory optimization
  - [ ] Profile SRAM/PSRAM usage
  - [ ] Optimize tensor arena size
  - [ ] Reduce model size if needed
- [ ] Latency optimization
  - [ ] Profile inference time
  - [ ] Use ESP-NN optimized ops
  - [ ] Multi-core utilization if beneficial
- [ ] Power optimization
  - [ ] Implement sleep modes
  - [ ] Reduce WiFi duty cycle
  - [ ] Measure battery life

## Phase 7: Documentation & Demo
- [ ] Polish web UI demo
  - [ ] Clean up interface design
  - [ ] Add controls for data collection/inference
  - [ ] Export results and visualizations
- [ ] Write technical report
  - [ ] Implementation details (laptop vs on-device)
  - [ ] Performance benchmarks
  - [ ] Comparison with paper results
- [ ] Create README with setup guide
  - [ ] Web UI usage instructions
  - [ ] ESP32 firmware setup
  - [ ] Model training guide
- [ ] Record demonstration video

---

## Notes
- Update this file as tasks are completed or new subtasks are discovered
- Use `git commit` to track progress over time
- Feel free to reorder or add tasks as the project evolves
