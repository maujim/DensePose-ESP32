#!/usr/bin/env python3
"""
Generate Synthetic CSI Dataset for Testing

This tool creates synthetic CSI data that simulates different human poses
for testing the ML pipeline when real hardware isn't available.

The synthetic data mimics the statistical properties of real CSI:
- Empty room: Low variance in amplitude and phase
- Person present: Higher variance, specific patterns
- Moving: High phase variance, temporal patterns
- Walking: Periodic patterns in amplitude
- Sitting/standing: Medium variance, distinct signatures

Usage:
    python3 generate_synthetic_data.py --output datasets/synthetic.json

Reference: DensePose From WiFi (arXiv:2301.00250)
"""

import sys
import json
import argparse
import numpy as np
from pathlib import Path
from datetime import datetime


class SyntheticCSIGenerator:
    """Generate synthetic CSI data that mimics real WiFi CSI patterns"""

    def __init__(self, num_subcarriers=52, sampling_rate=100):
        self.num_subcarriers = num_subcarriers
        self.sampling_rate = sampling_rate

    def generate_empty_room(self, num_samples=100):
        """
        Generate CSI data for empty room.

        Characteristics:
        - Low amplitude variance (< 3.0)
        - Low phase variance (< 0.1)
        - Stable RSSI around -50 dBm
        """
        samples = []
        for _ in range(num_samples):
            # Base amplitude with small noise
            amp = np.random.normal(20.0, 1.5, self.num_subcarriers)
            amp = np.clip(amp, 0, None)

            # Phase with minimal variation
            phase = np.random.normal(0, 0.05, self.num_subcarriers)

            sample = {
                'ts': np.random.randint(100000, 999999),
                'rssi': np.random.normal(-50, 2),
                'num': self.num_subcarriers,
                'amp': amp.tolist(),
                'phase': phase.tolist(),
                'label': 'empty',
                'description': 'Empty room - no person present'
            }
            samples.append(sample)

        return samples

    def generate_person_present(self, num_samples=100):
        """
        Generate CSI data for person present but static.

        Characteristics:
        - Medium amplitude variance (5-8)
        - Medium phase variance (0.3-0.5)
        - Slightly higher RSSI (-45 to -40 dBm)
        """
        samples = []
        for _ in range(num_samples):
            # Amplitude with medium variance
            amp = np.random.normal(25.0, 3.0, self.num_subcarriers)
            amp = np.clip(amp, 0, None)

            # Phase with moderate variation
            phase = np.random.normal(0, 0.2, self.num_subcarriers)

            sample = {
                'ts': np.random.randint(100000, 999999),
                'rssi': np.random.normal(-42, 3),
                'num': self.num_subcarriers,
                'amp': amp.tolist(),
                'phase': phase.tolist(),
                'label': 'present',
                'description': 'Person present, sitting or standing still'
            }
            samples.append(sample)

        return samples

    def generate_moving(self, num_samples=100):
        """
        Generate CSI data for moving person.

        Characteristics:
        - High amplitude variance (> 8)
        - High phase variance (> 0.5)
        - Fluctuating RSSI
        - Temporal patterns in amplitude
        """
        samples = []
        phase_offset = 0.0

        for i in range(num_samples):
            # Amplitude with high variance and temporal pattern
            base = 25.0 + 5.0 * np.sin(2 * np.pi * i / 20)
            amp = np.random.normal(base, 4.0, self.num_subcarriers)
            amp = np.clip(amp, 0, None)

            # Phase with high variance and drift
            phase_offset += np.random.normal(0, 0.1)
            phase = np.random.normal(phase_offset, 0.3, self.num_subcarriers)
            phase = np.clip(phase, -np.pi, np.pi)

            sample = {
                'ts': np.random.randint(100000, 999999),
                'rssi': np.random.normal(-40, 5),
                'num': self.num_subcarriers,
                'amp': amp.tolist(),
                'phase': phase.tolist(),
                'label': 'moving',
                'description': 'Person moving around'
            }
            samples.append(sample)

        return samples

    def generate_walking(self, num_samples=100):
        """
        Generate CSI data for walking person.

        Characteristics:
        - Periodic amplitude pattern (walking rhythm)
        - High phase variance
        - RSSI fluctuations
        - Distinctive periodic signature
        """
        samples = []

        for i in range(num_samples):
            # Walking creates periodic patterns (step frequency ~1-2 Hz)
            walking_phase = 2 * np.pi * i / 10  # ~10 samples per step
            amplitude_mod = 8.0 * np.sin(walking_phase) + 3.0 * np.sin(2 * walking_phase)

            amp = np.random.normal(25.0 + amplitude_mod, 3.5, self.num_subcarriers)
            amp = np.clip(amp, 0, None)

            # Phase variance correlates with steps
            phase = np.random.normal(0, 0.25 + 0.15 * np.sin(walking_phase), self.num_subcarriers)
            phase = np.clip(phase, -np.pi, np.pi)

            sample = {
                'ts': np.random.randint(100000, 999999),
                'rssi': np.random.normal(-38, 4),
                'num': self.num_subcarriers,
                'amp': amp.tolist(),
                'phase': phase.tolist(),
                'label': 'walking',
                'description': 'Person walking back and forth'
            }
            samples.append(sample)

        return samples

    def generate_sitting(self, num_samples=100):
        """
        Generate CSI data for sitting person.

        Characteristics:
        - Medium amplitude variance (4-6)
        - Low-medium phase variance (0.2-0.3)
        - Stable RSSI
        - More consistent than standing
        """
        samples = []

        for i in range(num_samples):
            # Slightly more stable than standing
            amp = np.random.normal(23.0, 2.2, self.num_subcarriers)
            amp = np.clip(amp, 0, None)

            # Lower phase variance than standing
            phase = np.random.normal(0, 0.15, self.num_subcarriers)

            sample = {
                'ts': np.random.randint(100000, 999999),
                'rssi': np.random.normal(-44, 2),
                'num': self.num_subcarriers,
                'amp': amp.tolist(),
                'phase': phase.tolist(),
                'label': 'sitting',
                'description': 'Person sitting still'
            }
            samples.append(sample)

        return samples

    def generate_standing(self, num_samples=100):
        """
        Generate CSI data for standing person.

        Characteristics:
        - Medium amplitude variance (5-7)
        - Medium phase variance (0.3-0.4)
        - Slight RSSI variation (minor movements)
        """
        samples = []

        for i in range(num_samples):
            # More variance than sitting (posture adjustments)
            amp = np.random.normal(24.0, 2.8, self.num_subcarriers)
            amp = np.clip(amp, 0, None)

            # Higher phase variance (minor movements)
            phase = np.random.normal(0, 0.22, self.num_subcarriers)

            sample = {
                'ts': np.random.randint(100000, 999999),
                'rssi': np.random.normal(-43, 2.5),
                'num': self.num_subcarriers,
                'amp': amp.tolist(),
                'phase': phase.tolist(),
                'label': 'standing',
                'description': 'Person standing still'
            }
            samples.append(sample)

        return samples

    def generate_dataset(self, samples_per_class=100):
        """Generate complete synthetic dataset"""
        print("Generating synthetic CSI dataset...")

        all_samples = []

        # Generate samples for each class
        print(f"  - Empty room: {samples_per_class} samples")
        all_samples.extend(self.generate_empty_room(samples_per_class))

        print(f"  - Person present: {samples_per_class} samples")
        all_samples.extend(self.generate_person_present(samples_per_class))

        print(f"  - Moving: {samples_per_class} samples")
        all_samples.extend(self.generate_moving(samples_per_class))

        print(f"  - Walking: {samples_per_class} samples")
        all_samples.extend(self.generate_walking(samples_per_class))

        print(f"  - Sitting: {samples_per_class} samples")
        all_samples.extend(self.generate_sitting(samples_per_class))

        print(f"  - Standing: {samples_per_class} samples")
        all_samples.extend(self.generate_standing(samples_per_class))

        # Shuffle samples
        np.random.shuffle(all_samples)

        # Create dataset structure
        dataset = {
            'metadata': {
                'generated_at': datetime.now().isoformat(),
                'total_packets': len(all_samples),
                'labels': list(set(s['label'] for s in all_samples)),
                'synthetic': True,
                'description': 'Synthetic CSI data for ML pipeline testing'
            },
            'data': all_samples
        }

        return dataset


def main():
    parser = argparse.ArgumentParser(
        description='Generate synthetic CSI dataset for testing ML pipeline',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate default dataset
  python3 generate_synthetic_data.py

  # Generate with custom output
  python3 generate_synthetic_data.py --output datasets/test.json

  # Generate more samples per class
  python3 generate_synthetic_data.py --samples-per-class 200

  # Generate with different subcarrier count
  python3 generate_synthetic_data.py --num-subcarriers 64

The synthetic data mimics real CSI statistical properties:
- Empty room: Low variance
- Person present: Medium variance
- Moving/Walking: High variance with patterns
        """
    )

    parser.add_argument('--output', default='datasets/synthetic_csi.json',
                       help='Output dataset file (default: datasets/synthetic_csi.json)')
    parser.add_argument('--samples-per-class', type=int, default=100,
                       help='Number of samples to generate per class (default: 100)')
    parser.add_argument('--num-subcarriers', type=int, default=52,
                       help='Number of WiFi subcarriers (default: 52)')
    parser.add_argument('--sampling-rate', type=int, default=100,
                       help='Sampling rate in Hz (default: 100)')

    args = parser.parse_args()

    # Create generator
    generator = SyntheticCSIGenerator(
        num_subcarriers=args.num_subcarriers,
        sampling_rate=args.sampling_rate
    )

    # Generate dataset
    dataset = generator.generate_dataset(args.samples_per_class)

    # Create output directory
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Save dataset
    print(f"\nSaving dataset to {output_path}...")
    with open(output_path, 'w') as f:
        json.dump(dataset, f, indent=2)

    print(f"✓ Dataset saved successfully")
    print(f"  Total samples: {dataset['metadata']['total_packets']}")
    print(f"  Classes: {', '.join(dataset['metadata']['labels'])}")
    print(f"  File size: {output_path.stat().st_size / 1024:.1f} KB")

    # Print statistics
    print("\nDataset statistics:")
    label_counts = {}
    for sample in dataset['data']:
        label = sample['label']
        label_counts[label] = label_counts.get(label, 0) + 1

    for label, count in sorted(label_counts.items()):
        print(f"  {label:15s}: {count:4d} samples")

    print("\nNext steps:")
    print("1. Analyze the synthetic data:")
    print("   python3 ../tools/analyze_csi.py", args.output)
    print("\n2. Train a model on synthetic data:")
    print("   python3 ../models/training/train_pose_model.py", args.output)
    print("\n3. Note: Synthetic data is for testing only!")
    print("   For production, collect real CSI data with collect_csi_dataset.py")


if __name__ == '__main__':
    main()
