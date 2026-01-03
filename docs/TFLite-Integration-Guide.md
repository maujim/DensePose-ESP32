# TensorFlow Lite Micro Integration Guide

This guide explains how to integrate TensorFlow Lite Micro into the DensePose-ESP32 project for on-device ML inference.

## Overview

The project uses TensorFlow Lite Micro to run pose estimation models directly on ESP32-S3 hardware. This enables real-time inference without cloud connectivity.

## Architecture

```
WiFi CSI Data → Feature Extraction → TFLite Model → Pose Classification
     ↓                  ↓                  ↓              ↓
  Raw I/Q         Temporal          INT8 Model       6 Classes
  52 subs         Windows           <500KB         (Empty, Present,
  @100Hz         (500ms)                           Moving, etc.)
```

## Implementation Approach

### Phase 1: Rule-Based Detection (CURRENT)

The current implementation uses statistical features for pose detection:
- Amplitude mean and standard deviation
- Phase variance
- RSSI values
- Motion detection thresholds

**Advantages:**
- No model training required
- Small code footprint
- Fast execution
- Works immediately

**Limitations:**
- Lower accuracy
- Limited pose classes
- Manual threshold tuning

### Phase 2: TFLite Micro Integration (FUTURE)

For production use with trained ML models:

#### Option A: ESP-DL (Recommended for ESP32)

Espressif's official deep learning library optimized for ESP32.

**Pros:**
- ESP-NN acceleration for Xtensa cores
- Better performance than generic TFLite
- Official Espressif support
- Smaller binary size

**Cons:**
- Less flexible than standard TFLite
- Fewer pre-trained models

**Setup:**
```bash
# Add ESP-DL component
cd firmware/components
git clone https://github.com/espressif/esp-dl.git

# Update sdkconfig
idf.py menuconfig
# Enable: Component config -> ESP-DL
```

#### Option B: Standard TFLite Micro

Generic TensorFlow Lite Micro for ESP-IDF.

**Pros:**
- Full TFLite ecosystem
- Easy model conversion
- Cross-platform compatibility

**Cons:**
- Larger binary size
- Slower than ESP-DL optimizations
- More complex build setup

**Setup:**
```bash
# Add as component to firmware/
cd firmware/main
# Download TFLite Micro for ESP32
# See: https://github.com/espressif/tflite-micro-esp-examples
```

## Model Preparation

### 1. Train Model

Use the training script in `models/training/`:

```bash
cd models/training
pip install -r requirements.txt
python3 train_pose_model.py ../../datasets/csi_dataset.json
```

This generates:
- `pose_model.keras`: Full Keras model
- `pose_model_float.tflite`: Float32 TFLite model
- `pose_model_quant.tflite`: INT8 quantized model (for ESP32)

### 2. Convert to C Header

Convert the quantized model to a C array for embedding in firmware:

```bash
# Tool: xxd (comes with Linux/Mac, or use hexdump on Windows)
xxd -i pose_model_quant.tflite > model_data.cc

# Or use Python script
python3 ../tools/convert_tflite_to_header.py \
    pose_model_quant.tflite \
    firmware/main/model_data.tflite.h
```

This creates a header file with the model as a byte array.

### 3. Integrate into Firmware

Add the model include to `pose_inference.c`:

```c
// Include the generated model header
#include "model_data.tflite.h"

// Model is now available as:
// extern const unsigned char model_data[];
// extern const unsigned int model_data_len;
```

## TFLite Micro Integration Code

Here's a simplified implementation template:

```c
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "model_data.tflite.h"

// Tensor arena size (tune based on model)
constexpr int kTensorArenaSize = 200 * 1024;  // 200KB
uint8_t tensor_arena[kTensorArenaSize];

// Load model
const tflite::Model* model = tflite::GetModel(g_model_data);

// Create resolver
tflite::MicroMutableOpResolver<5> resolver;
resolver.AddFullyConnected();
resolver.AddConv2D();
resolver.AddAveragePooling2D();
resolver.AddReshape();
resolver.AddSoftmax();

// Create interpreter
tflite::MicroInterpreter interpreter(model, resolver,
                                     tensor_arena, kTensorArenaSize);

// Allocate tensors
interpreter.AllocateTensors();

// Get input/output pointers
TfLiteTensor* input = interpreter.input(0);
TfLiteTensor* output = interpreter.output(0);

// Run inference
memcpy(input->data.int8, input_data, input_size);
interpreter.Invoke();
int8_t result = output->data.int8[0];
```

## Memory Requirements

Based on model size and architecture:

| Component | Usage | Notes |
|-----------|-------|-------|
| Model weights | 200-500KB | In flash |
| Tensor arena | 100-200KB | In internal RAM |
| CSI buffers | ~100KB | In PSRAM |
| Code | ~50KB | In flash |
| **Total** | ~800KB | Within ESP32-S3 capacity |

## Build Configuration

Enable required options in `sdkconfig`:

```
# Enable PSRAM
CONFIG_ESP32S3_SPIRAM_SUPPORT=y

# Enable WiFi CSI
CONFIG_WIFI_CSI_SUPPORT=y

# Optimizations
CONFIG_COMPILER_OPTIMIZATION_SIZE=y

# Logging (reduce in production)
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

## Performance Expectations

Based on ESP32-S3 specs (240MHz dual-core):

| Metric | Target | Notes |
|--------|--------|-------|
| Inference time | <100ms | Per 500ms window |
| Memory usage | <500KB | Including model |
| Power consumption | <500mA | During inference |
| Accuracy | >85% | On 6-class classification |

## Testing

### 1. Unit Tests

Test model inference with known inputs:

```c
// Test with empty room data
TEST_F(PoseInferenceTest, EmptyRoom) {
    float empty_input[50][104] = {/* low variance data */};
    pose_class_t result = pose_classify(empty_input);
    EXPECT_EQ(result, POSE_EMPTY);
}

// Test with person present
TEST_F(PoseInferenceTest, PersonPresent) {
    float present_input[50][104] = {/* high variance data */};
    pose_class_t result = pose_classify(present_input);
    EXPECT_EQ(result, POSE_PRESENT);
}
```

### 2. Integration Tests

Test full pipeline:

1. Flash firmware with model
2. Connect to serial monitor
3. Perform poses in front of ESP32
4. Verify classification accuracy

### 3. Benchmark Tests

Measure inference latency and memory:

```c
uint64_t start = esp_timer_get_time();
pose_process_csi(csi_data);
uint64_t end = esp_timer_get_time();

ESP_LOGI(TAG, "Inference: %llu us", end - start);
```

## Troubleshooting

### Model Too Large

**Problem**: Model exceeds 500KB target

**Solutions:**
1. Reduce window size (50→30 samples)
2. Reduce number of filters (64→32)
3. Use fewer layers
4. Increase quantization (INT8 → INT4)

### Out of Memory

**Problem**: Tensor arena allocation fails

**Solutions:**
1. Use PSRAM for tensor arena
2. Reduce input size
3. Process smaller batches
4. Free unused buffers

### Poor Accuracy

**Problem**: Model misclassifies frequently

**Solutions:**
1. Collect more training data
2. Balance classes
3. Tune quantization parameters
4. Use representative dataset for calibration

### Slow Inference

**Problem**: Inference >100ms

**Solutions:**
1. Use ESP-DL instead of generic TFLite
2. Enable -O2 optimization
3. Reduce model complexity
4. Use second core for inference

## Current Implementation Status

**Phase 1 (Complete):**
- ✅ CSI data collection
- ✅ Statistical feature extraction
- ✅ Rule-based pose detection
- ✅ Serial data streaming
- ✅ Python data collection tools

**Phase 2 (In Progress):**
- ⏳ ML model training pipeline
- ⏳ TFLite model conversion
- ⏳ Firmware integration
- ⏳ Performance optimization

**Phase 3 (Future):**
- ⏳ Multi-person detection
- ⏳ DensePose UV coordinates
- ⏳ Online model updates
- ⏳ Multi-board arrays

## References

- [TFLite Micro ESP32 Examples](https://github.com/espressif/tflite-micro-esp-examples)
- [ESP-DL Documentation](https://github.com/espressif/esp-dl)
- [TFLite Micro GitHub](https://github.com/tensorflow/tflite-micro)
- [Paper: DensePose From WiFi](https://arxiv.org/abs/2301.00250)

## Next Steps

1. **Collect Training Data**: Use `tools/collect_csi_dataset.py` to gather labeled data
2. **Train Model**: Run `models/training/train_pose_model.py`
3. **Convert Model**: Generate `.tflite` and `.h` files
4. **Integrate**: Update `pose_inference.c` with TFLite code
5. **Test**: Flash and validate on hardware

For questions or issues, refer to the main README or open an issue on GitHub.
