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
- [ ] Data logging and visualization
  - [ ] Add serial output for CSI data
  - [ ] Create Python script to capture serial data
  - [ ] Plot amplitude/phase over time
  - [ ] Test with/without human presence

## Phase 3: Signal Processing & Feature Extraction
- [ ] Implement noise filtering
  - [ ] Remove DC offset
  - [ ] Apply moving average filter
  - [ ] Handle outliers/bad packets
- [ ] Feature extraction from CSI
  - [ ] Time-domain features (variance, mean)
  - [ ] Frequency analysis (FFT if needed)
  - [ ] Subcarrier selection (identify most sensitive)
- [ ] Data collection for training
  - [ ] Set up controlled environment
  - [ ] Collect CSI samples (empty room baseline)
  - [ ] Collect CSI with human present
  - [ ] Label different poses/activities

## Phase 4: Human Presence Detection (Simplified ML)
- [ ] Training pipeline (on PC)
  - [ ] Prepare dataset (CSI → binary label: present/absent)
  - [ ] Train simple classifier (Random Forest, SVM, or small NN)
  - [ ] Evaluate accuracy
  - [ ] Convert model to TFLite
  - [ ] Quantize to INT8
- [ ] Deploy to ESP32
  - [ ] Integrate TFLite Micro
  - [ ] Load model from flash
  - [ ] Run inference on CSI data
  - [ ] Test latency and memory usage
- [ ] Validation
  - [ ] Test detection accuracy
  - [ ] Measure power consumption
  - [ ] Optimize if needed

## Phase 5: Pose Estimation (Full Implementation)
- [ ] Research model architecture
  - [ ] Review DensePose paper network design
  - [ ] Design ESP32-compatible simplified version
  - [ ] Determine feasibility (memory/compute constraints)
- [ ] Data collection for pose training
  - [ ] Set up camera + WiFi synchronized recording
  - [ ] Collect paired CSI + pose labels
  - [ ] Generate DensePose annotations from images
- [ ] Model training
  - [ ] Train pose estimation model
  - [ ] Knowledge distillation for size reduction
  - [ ] Quantization for ESP32 deployment
- [ ] Deployment and testing
  - [ ] Flash model to ESP32
  - [ ] Real-time pose inference
  - [ ] Benchmark performance

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
- [ ] Create demo application
  - [ ] Serial output visualization
  - [ ] Optional: BLE/WiFi output to phone
- [ ] Write technical report
  - [ ] Implementation details
  - [ ] Performance benchmarks
  - [ ] Comparison with paper results
- [ ] Create README with setup guide
- [ ] Record demonstration video

---

## Notes
- Update this file as tasks are completed or new subtasks are discovered
- Use `git commit` to track progress over time
- Feel free to reorder or add tasks as the project evolves
