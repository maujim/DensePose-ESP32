# Machine Learning Models for DensePose-ESP32

This directory contains the machine learning pipeline for training and deploying WiFi-based pose estimation models on ESP32-S3.

## Overview

The implementation follows the "DensePose From WiFi" paper (arXiv:2301.00250), adapted for ESP32-S3 hardware constraints:

- **Input**: Temporal windows of WiFi CSI data (amplitude + phase)
- **Output**: Pose classification (empty, present, moving, walking, sitting, standing)
- **Target Size**: <500KB for model weights
- **Quantization**: INT8 for efficient on-device inference

## Directory Structure

```
models/
├── training/              # Training scripts and utilities
│   ├── train_pose_model.py    # Main training script
│   ├── requirements.txt        # Python dependencies
│   └── output/                 # Trained models (generated)
└── tflite/               # TFLite Micro models for ESP32
    ├── pose_model_float.tflite   # Float32 model (testing)
    └── pose_model_quant.tflite   # INT8 quantized model (production)
```

## Training Pipeline

### 1. Collect CSI Data

Use the data collection tool in `../tools/collect_csi_dataset.py`:

```bash
cd ../tools
python3 collect_csi_dataset.py /dev/ttyUSB0
```

This will create labeled datasets in the `datasets/` directory.

### 2. Train Model

```bash
cd models/training
pip install -r requirements.txt
python3 train_pose_model.py ../../datasets/csi_dataset_20240103_120000.json
```

Training options:
- `--window-size 50`: Temporal window size in samples (500ms at 100Hz)
- `--epochs 50`: Number of training epochs
- `--batch-size 32`: Batch size for training
- `--output-dir`: Output directory for trained models

### 3. Model Output

The training script generates:

1. **pose_model.keras**: Full Keras model (for further training)
2. **pose_model_float.tflite**: TFLite float32 model (for testing)
3. **pose_model_quant.tflite**: TFLite INT8 model (for ESP32 deployment)
4. **model_summary.txt**: Model architecture summary

### 4. Deploy to ESP32

Copy the quantized model to the firmware:

```bash
cp models/training/output/pose_model_quant.tflite firmware/main/model_data.tflite
```

Then rebuild and flash the firmware.

## Model Architecture

The lightweight CNN architecture:

```
Input: (window_size, num_subcarriers * 2)
  ↓
Conv1D(32, kernel=5) + BatchNorm + MaxPool
  ↓
Conv1D(64, kernel=5) + BatchNorm + MaxPool
  ↓
Conv1D(64, kernel=3) + BatchNorm + GlobalAvgPool
  ↓
Dense(64) + Dropout(0.3)
  ↓
Dense(32) + Dropout(0.2)
  ↓
Output: Dense(num_classes)
```

**Key design decisions:**
- **Conv1D layers**: Capture temporal patterns in CSI data
- **Batch normalization**: Stabilize training with varying signal scales
- **Global average pooling**: Reduce parameters vs. flatten + dense
- **Dropout**: Prevent overfitting on small datasets

## Model Performance

Target metrics (based on paper and hardware constraints):

- **Model size**: <500KB (quantized)
- **Inference time**: <100ms per window on ESP32-S3
- **Accuracy**: >85% on 6-class classification
- **Memory usage**: <200KB tensor arena

## Data Requirements

For good model performance:

1. **Minimum 100 samples per class**: More is better
2. **Balanced classes**: Equal samples per pose type
3. **Environment consistency**: Train in same environment as deployment
4. **Multiple subjects**: Different people for generalization

## Troubleshooting

### Training Issues

**Low accuracy (<70%):**
- Collect more training data
- Balance classes (equal samples per pose)
- Increase model capacity (add more filters)
- Try longer temporal windows

**Overfitting (high train, low val):**
- Add more dropout
- Collect more diverse data
- Use data augmentation (add noise, shift signals)

**Quantization accuracy drop:**
- Use representative dataset for calibration
- Ensure training data matches deployment distribution
- Try QAT (Quantization-Aware Training)

### Deployment Issues

**Model too large:**
- Reduce window size
- Reduce number of filters (32→16→8)
- Use fewer layers

**Out of memory:**
- Reduce tensor arena size
- Process smaller windows
- Use PSRAM for model weights

## References

- Paper: [DensePose From WiFi](https://arxiv.org/abs/2301.00250)
- TFLite Micro: [GitHub](https://github.com/tensorflow/tflite-micro)
- ESP-DL: [Espressif Deep Learning](https://github.com/espressif/esp-dl)

## Future Improvements

1. **Multi-person detection**: Track multiple people simultaneously
2. **DensePose output**: Full UV coordinate prediction (requires more compute)
3. **Online learning**: Update model on-device
4. **Multi-board array**: Use multiple ESP32s for better accuracy
