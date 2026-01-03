#!/usr/bin/env python3
"""
Train Pose Estimation Model from CSI Dataset

This script trains a lightweight neural network for WiFi-based human pose estimation,
following the "DensePose From WiFi" paper approach, simplified for ESP32 constraints.

The model uses temporal CSI windows to predict:
1. Human presence (binary classification)
2. Basic pose classes (empty, present, moving, walking, sitting, standing)

Usage:
    python3 train_pose_model.py datasets/csi_dataset_20240103_120000.json

Reference: "DensePose From WiFi" (arXiv:2301.00250)
"""

import sys
import json
import argparse
from pathlib import Path
import numpy as np
from datetime import datetime

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers

# Set random seeds for reproducibility
np.random.seed(42)
tf.random.set_seed(42)


class CSIDataPreprocessor:
    """
    Preprocess CSI data for neural network training.

    The paper uses temporal windows of CSI data as input.
    For ESP32 constraints, we use a simplified approach:
    - Window size: 50 samples (500ms at 100Hz)
    - Features per sample: amplitude (52) + phase (52) = 104
    - Input shape: (50, 104)
    """

    def __init__(self, window_size=50, num_subcarriers=52):
        self.window_size = window_size
        self.num_subcarriers = num_subcarriers
        self.feature_dim = num_subcarriers * 2  # amplitude + phase

    def parse_sample(self, sample):
        """Parse a single CSI sample into features"""
        amp = np.array(sample['amp'], dtype=np.float32)
        phase = np.array(sample.get('phase', [0] * len(amp)), dtype=np.float32)

        # Pad or truncate to fixed size
        if len(amp) < self.num_subcarriers:
            amp = np.pad(amp, (0, self.num_subcarriers - len(amp)), 'constant')
            phase = np.pad(phase, (0, self.num_subcarriers - len(phase)), 'constant')
        else:
            amp = amp[:self.num_subcarriers]
            phase = phase[:self.num_subcarriers]

        # Normalize amplitude
        amp = (amp - np.mean(amp)) / (np.std(amp) + 1e-6)

        # Normalize phase to [-1, 1]
        phase = phase / np.pi

        # Concatenate amplitude and phase
        features = np.concatenate([amp, phase])
        return features

    def create_windows(self, samples, labels):
        """
        Create temporal windows from samples.

        Returns:
            X: (num_windows, window_size, feature_dim)
            y: (num_windows,) labels
        """
        X = []
        y = []

        label_map = {
            'empty': 0,
            'present': 1,
            'moving': 2,
            'walking': 3,
            'sitting': 4,
            'standing': 5,
        }

        # Group samples by label
        by_label = {}
        for sample, label in zip(samples, labels):
            if label not in by_label:
                by_label[label] = []
            by_label[label].append(sample)

        # Create windows for each label
        for label, label_samples in by_label.items():
            features = [self.parse_sample(s) for s in label_samples]
            features = np.array(features)

            # Create sliding windows
            for i in range(len(features) - self.window_size + 1):
                window = features[i:i + self.window_size]
                X.append(window)
                y.append(label_map.get(label, 0))

        return np.array(X), np.array(y)

    def load_dataset(self, dataset_file):
        """Load and preprocess dataset from JSON file"""
        print(f"Loading dataset from {dataset_file}...")

        with open(dataset_file, 'r') as f:
            dataset = json.load(f)

        data = dataset['data']
        print(f"  Loaded {len(data)} samples")

        # Extract samples and labels
        samples = []
        labels = []
        for sample in data:
            if 'label' in sample and 'amp' in sample:
                samples.append(sample)
                labels.append(sample['label'])

        print(f"  Labeled samples: {len(samples)}")

        # Create windows
        X, y = self.create_windows(samples, labels)
        print(f"  Created {len(X)} temporal windows (size={self.window_size})")

        return X, y


class LightweightPoseModel:
    """
    Lightweight CNN model for WiFi-based pose estimation.

    Architecture inspired by the DensePose from WiFi paper but simplified
    for ESP32-S3 constraints (2MB PSRAM, 240MHz dual-core).

    Model size target: <500KB
    Input: (window_size, num_subcarriers * 2)
    Output: 6 pose classes
    """

    def __init__(self, input_shape, num_classes=6):
        self.input_shape = input_shape
        self.num_classes = num_classes

    def build_model(self):
        """Build lightweight CNN model"""

        inputs = keras.Input(shape=self.input_shape)

        # Temporal convolution layers
        x = layers.Conv1D(32, 5, activation='relu', padding='same')(inputs)
        x = layers.BatchNormalization()(x)
        x = layers.MaxPooling1D(2)(x)

        x = layers.Conv1D(64, 5, activation='relu', padding='same')(x)
        x = layers.BatchNormalization()(x)
        x = layers.MaxPooling1D(2)(x)

        x = layers.Conv1D(64, 3, activation='relu', padding='same')(x)
        x = layers.BatchNormalization()(x)
        x = layers.GlobalAveragePooling1D()(x)

        # Dense layers
        x = layers.Dense(64, activation='relu')(x)
        x = layers.Dropout(0.3)(x)

        x = layers.Dense(32, activation='relu')(x)
        x = layers.Dropout(0.2)(x)

        # Output layer
        if self.num_classes == 2:
            outputs = layers.Dense(1, activation='sigmoid')(x)
        else:
            outputs = layers.Dense(self.num_classes, activation='softmax')(x)

        model = keras.Model(inputs=inputs, outputs=outputs, name='LightweightPoseModel')

        return model


def train_model(X_train, y_train, X_val, y_val, num_classes=6, epochs=50, batch_size=32):
    """Train the pose estimation model"""

    print("\n" + "="*60)
    print("TRAINING MODEL")
    print("="*60)

    input_shape = (X_train.shape[1], X_train.shape[2])

    builder = LightweightPoseModel(input_shape, num_classes)
    model = builder.build_model()

    model.summary()

    # Compile model
    if num_classes == 2:
        loss = 'binary_crossentropy'
        metrics = ['accuracy', keras.metrics.Precision(), keras.metrics.Recall()]
    else:
        loss = 'sparse_categorical_crossentropy'
        metrics = ['accuracy']

    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=0.001),
        loss=loss,
        metrics=metrics
    )

    # Callbacks
    callbacks = [
        keras.callbacks.EarlyStopping(
            monitor='val_loss',
            patience=10,
            restore_best_weights=True
        ),
        keras.callbacks.ReduceLROnPlateau(
            monitor='val_loss',
            factor=0.5,
            patience=5
        )
    ]

    # Train
    print("\nStarting training...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=epochs,
        batch_size=batch_size,
        callbacks=callbacks,
        verbose=1
    )

    # Evaluate
    print("\n" + "="*60)
    print("EVALUATION")
    print("="*60)

    train_loss, train_acc = model.evaluate(X_train, y_train, verbose=0)
    val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)

    print(f"Train Accuracy: {train_acc:.4f}")
    print(f"Val Accuracy:   {val_acc:.4f}")

    return model, history


def save_model(model, output_dir):
    """Save model in multiple formats"""

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    # Save as Keras model
    keras_path = output_dir / 'pose_model.keras'
    model.save(keras_path)
    print(f"\n✓ Saved Keras model to {keras_path}")

    # Save as TFLite float32 model
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = []
    tflite_float_path = output_dir / 'pose_model_float.tflite'
    with open(tflite_float_path, 'wb') as f:
        f.write(converter.convert())
    print(f"✓ Saved TFLite float32 model to {tflite_float_path}")

    # Save as TFLite quantized INT8 model (for ESP32)
    def representative_dataset():
        # We'll need a small sample for calibration
        # This is just a placeholder - actual data should be used
        for _ in range(100):
            yield [np.random.rand(1, 50, 104).astype(np.float32)]

    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    try:
        tflite_quant_path = output_dir / 'pose_model_quant.tflite'
        with open(tflite_quant_path, 'wb') as f:
            f.write(converter.convert())
        print(f"✓ Saved TFLite quantized INT8 model to {tflite_quant_path}")
    except Exception as e:
        print(f"⚠ Quantization failed: {e}")
        print("  The float32 model can still be used")

    # Save model summary
    summary_path = output_dir / 'model_summary.txt'
    with open(summary_path, 'w') as f:
        model.summary(print_fn=lambda x: f.write(x + '\n'))
    print(f"✓ Saved model summary to {summary_path}")

    # Print model sizes
    print("\nModel Sizes:")
    print(f"  Keras:     {keras_path.stat().st_size / 1024:.1f} KB")
    print(f"  TFLite:    {tflite_float_path.stat().st_size / 1024:.1f} KB")
    if (output_dir / 'pose_model_quant.tflite').exists():
        print(f"  Quantized: {(output_dir / 'pose_model_quant.tflite').stat().st_size / 1024:.1f} KB")


def main():
    parser = argparse.ArgumentParser(
        description='Train WiFi CSI pose estimation model',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Example:
  python3 train_pose_model.py datasets/csi_dataset.json --epochs 50

  The trained model will be saved in models/training/output/
        """
    )

    parser.add_argument('dataset', help='Dataset JSON file')
    parser.add_argument('--window-size', type=int, default=50, help='Temporal window size')
    parser.add_argument('--num-subcarriers', type=int, default=52, help='Number of WiFi subcarriers')
    parser.add_argument('--train-split', type=float, default=0.8, help='Training data fraction')
    parser.add_argument('--epochs', type=int, default=50, help='Number of training epochs')
    parser.add_argument('--batch-size', type=int, default=32, help='Batch size')
    parser.add_argument('--output-dir', default='models/training/output', help='Output directory')

    args = parser.parse_args()

    # Load and preprocess data
    preprocessor = CSIDataPreprocessor(args.window_size, args.num_subcarriers)
    X, y = preprocessor.load_dataset(args.dataset)

    if len(X) == 0:
        print("✗ No valid data found in dataset")
        sys.exit(1)

    # Split data
    split_idx = int(len(X) * args.train_split)
    X_train, X_val = X[:split_idx], X[split_idx:]
    y_train, y_val = y[:split_idx], y[split_idx:]

    print(f"\nData split:")
    print(f"  Train: {len(X_train)} samples")
    print(f"  Val:   {len(X_val)} samples")

    # Get number of classes
    num_classes = len(np.unique(y))
    print(f"  Classes: {num_classes}")

    # Train model
    model, history = train_model(
        X_train, y_train,
        X_val, y_val,
        num_classes=num_classes,
        epochs=args.epochs,
        batch_size=args.batch_size
    )

    # Save model
    save_model(model, args.output_dir)

    print("\n✓ Training complete!")


if __name__ == '__main__':
    main()
