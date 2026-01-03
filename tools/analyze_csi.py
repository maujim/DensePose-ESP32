#!/usr/bin/env python3
"""
CSI Data Analysis Tool

Analyzes collected CSI datasets, computes features, and prepares data for ML training.

Usage:
    python3 analyze_csi.py datasets/csi_dataset_20240103_120000.json
"""

import sys
import json
import argparse
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt


class CSIAnalyzer:
    def __init__(self, dataset_file):
        self.dataset_file = Path(dataset_file)
        self.data = None
        self.metadata = None

    def load(self):
        """Load dataset from JSON file"""
        print(f"Loading {self.dataset_file}...")
        with open(self.dataset_file, 'r') as f:
            dataset = json.load(f)

        self.metadata = dataset['metadata']
        self.data = dataset['data']

        print(f"✓ Loaded {len(self.data)} packets")
        print(f"  Labels: {', '.join(self.metadata['labels'])}")
        return self

    def compute_features(self, sample):
        """Compute features from a single CSI sample"""
        amp = np.array(sample['amp'])
        phase = np.array(sample.get('phase', []))

        features = {
            # Amplitude statistics
            'amp_mean': np.mean(amp),
            'amp_std': np.std(amp),
            'amp_var': np.var(amp),
            'amp_min': np.min(amp),
            'amp_max': np.max(amp),
            'amp_range': np.ptp(amp),
            'amp_median': np.median(amp),
            'amp_q25': np.percentile(amp, 25),
            'amp_q75': np.percentile(amp, 75),

            # Phase statistics
            'phase_mean': np.mean(phase) if len(phase) > 0 else 0,
            'phase_std': np.std(phase) if len(phase) > 0 else 0,
            'phase_var': np.var(phase) if len(phase) > 0 else 0,

            # Signal metadata
            'rssi': sample.get('rssi', 0),
            'num_subcarriers': sample.get('num', len(amp)),
        }

        return features

    def analyze_by_label(self):
        """Analyze features grouped by label"""
        print("\n" + "="*60)
        print("ANALYSIS BY LABEL")
        print("="*60)

        label_groups = {}
        for sample in self.data:
            label = sample.get('label', 'unknown')
            if label not in label_groups:
                label_groups[label] = []
            label_groups[label].append(sample)

        for label, samples in sorted(label_groups.items()):
            print(f"\n[{label.upper()}] - {len(samples)} samples")

            # Compute features for all samples
            features_list = [self.compute_features(s) for s in samples]

            # Aggregate statistics
            amp_means = [f['amp_mean'] for f in features_list]
            amp_stds = [f['amp_std'] for f in features_list]
            phase_stds = [f['phase_std'] for f in features_list]

            print(f"  Amplitude Mean: {np.mean(amp_means):.2f} ± {np.std(amp_means):.2f}")
            print(f"  Amplitude Std:  {np.mean(amp_stds):.2f} ± {np.std(amp_stds):.2f}")
            print(f"  Phase Std:      {np.mean(phase_stds):.4f} ± {np.std(phase_stds):.4f}")
            print(f"  RSSI:           {np.mean([f['rssi'] for f in features_list]):.1f} dBm")

    def compute_temporal_features(self, window_size=10):
        """Compute temporal features over sliding windows"""
        print("\n" + "="*60)
        print("TEMPORAL FEATURE ANALYSIS")
        print("="*60)

        label_groups = {}
        for sample in self.data:
            label = sample.get('label', 'unknown')
            if label not in label_groups:
                label_groups[label] = []

            features = self.compute_features(sample)
            label_groups[label].append(features)

        # Compute temporal variance for each label
        for label, features_list in sorted(label_groups.items()):
            if len(features_list) < window_size:
                print(f"\n[{label}] - Not enough samples for temporal analysis")
                continue

            print(f"\n[{label.upper()}]")

            # Compute sliding window statistics
            amp_std_variance = []
            phase_std_variance = []

            for i in range(len(features_list) - window_size + 1):
                window = features_list[i:i+window_size]

                # Variance of amplitude std within window
                amp_std_variance.append(np.std([f['amp_std'] for f in window]))
                phase_std_variance.append(np.std([f['phase_std'] for f in window]))

            print(f"  Amplitude Std Variance: {np.mean(amp_std_variance):.4f}")
            print(f"  Phase Std Variance:     {np.mean(phase_std_variance):.4f}")
            print(f"  Temporal Stability:     {np.mean(amp_std_variance) < 5.0}")

    def visualize(self, output_dir=None):
        """Generate visualizations of the dataset"""
        print("\n" + "="*60)
        print("GENERATING VISUALIZATIONS")
        print("="*60)

        if output_dir is None:
            output_dir = self.dataset_file.parent / 'visualizations'

        output_dir = Path(output_dir)
        output_dir.mkdir(exist_ok=True)

        # Group by label
        label_groups = {}
        for sample in self.data:
            label = sample.get('label', 'unknown')
            if label not in label_groups:
                label_groups[label] = {'amp': [], 'phase': []}

            label_groups[label]['amp'].append(sample['amp'])
            if 'phase' in sample:
                label_groups[label]['phase'].append(sample['phase'])

        # Plot 1: Amplitude distribution by label
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))

        # Amplitude means
        ax = axes[0, 0]
        for label, data in sorted(label_groups.items()):
            amp_means = [np.mean(amp) for amp in data['amp']]
            ax.hist(amp_means, alpha=0.6, label=label, bins=30)
        ax.set_xlabel('Amplitude Mean')
        ax.set_ylabel('Frequency')
        ax.set_title('Distribution of Amplitude Means by Label')
        ax.legend()
        ax.grid(True, alpha=0.3)

        # Amplitude std
        ax = axes[0, 1]
        for label, data in sorted(label_groups.items()):
            amp_stds = [np.std(amp) for amp in data['amp']]
            ax.hist(amp_stds, alpha=0.6, label=label, bins=30)
        ax.set_xlabel('Amplitude Std Dev')
        ax.set_ylabel('Frequency')
        ax.set_title('Distribution of Amplitude Std Dev by Label')
        ax.legend()
        ax.grid(True, alpha=0.3)

        # RSSI by label
        ax = axes[1, 0]
        for label, data in sorted(label_groups.items()):
            rssis = [s.get('rssi', 0) for s in self.data if s.get('label') == label]
            ax.hist(rssis, alpha=0.6, label=label, bins=30)
        ax.set_xlabel('RSSI (dBm)')
        ax.set_ylabel('Frequency')
        ax.set_title('RSSI Distribution by Label')
        ax.legend()
        ax.grid(True, alpha=0.3)

        # Sample amplitude waveform
        ax = axes[1, 1]
        for i, (label, data) in enumerate(sorted(label_groups.items())[:3]):
            if data['amp']:
                sample_amp = data['amp'][0]
                ax.plot(sample_amp[:30], alpha=0.7, label=f'{label} (sample)')
        ax.set_xlabel('Subcarrier Index')
        ax.set_ylabel('Amplitude')
        ax.set_title('Sample Amplitude Waveforms (first 30 subcarriers)')
        ax.legend()
        ax.grid(True, alpha=0.3)

        plt.tight_layout()
        fig.savefig(output_dir / 'feature_distributions.png', dpi=150)
        print(f"✓ Saved feature distributions to {output_dir / 'feature_distributions.png'}")

        # Plot 2: Temporal evolution
        fig, axes = plt.subplots(2, 1, figsize=(14, 8))

        # Amplitude mean over time
        ax = axes[0]
        for label in sorted(label_groups.keys())[:3]:
            samples = [s for s in self.data if s.get('label') == label]
            if samples:
                amp_means = [np.mean(s['amp']) for s in samples]
                ax.plot(amp_means, alpha=0.7, label=label)
        ax.set_xlabel('Sample Index')
        ax.set_ylabel('Amplitude Mean')
        ax.set_title('Temporal Evolution of Amplitude Means')
        ax.legend()
        ax.grid(True, alpha=0.3)

        # Amplitude std over time
        ax = axes[1]
        for label in sorted(label_groups.keys())[:3]:
            samples = [s for s in self.data if s.get('label') == label]
            if samples:
                amp_stds = [np.std(s['amp']) for s in samples]
                ax.plot(amp_stds, alpha=0.7, label=label)
        ax.set_xlabel('Sample Index')
        ax.set_ylabel('Amplitude Std Dev')
        ax.set_title('Temporal Evolution of Amplitude Std Dev')
        ax.legend()
        ax.grid(True, alpha=0.3)

        plt.tight_layout()
        fig.savefig(output_dir / 'temporal_evolution.png', dpi=150)
        print(f"✓ Saved temporal evolution to {output_dir / 'temporal_evolution.png'}")

        print(f"\n✓ All visualizations saved to {output_dir}")

    def export_features(self, output_file=None):
        """Export computed features for ML training"""
        if output_file is None:
            output_file = self.dataset_file.parent / f'{self.dataset_file.stem}_features.json'

        features_data = []
        for sample in self.data:
            features = self.compute_features(sample)
            features['label'] = sample.get('label', 'unknown')
            features['timestamp'] = sample.get('ts', 0)
            features_data.append(features)

        with open(output_file, 'w') as f:
            json.dump({
                'metadata': {
                    'source': str(self.dataset_file),
                    'num_samples': len(features_data),
                    'features': list(features_data[0].keys()) if features_data else []
                },
                'data': features_data
            }, f, indent=2)

        print(f"✓ Exported features to {output_file}")
        return output_file


def main():
    parser = argparse.ArgumentParser(
        description='Analyze CSI dataset and compute features for ML training'
    )
    parser.add_argument('dataset', help='Dataset JSON file')
    parser.add_argument('--visualize', action='store_true', help='Generate visualizations')
    parser.add_argument('--output-dir', help='Output directory for visualizations')
    parser.add_argument('--export-features', action='store_true', help='Export computed features')

    args = parser.parse_args()

    analyzer = CSIAnalyzer(args.dataset).load()
    analyzer.analyze_by_label()
    analyzer.compute_temporal_features()

    if args.visualize:
        analyzer.visualize(args.output_dir)

    if args.export_features:
        analyzer.export_features()


if __name__ == '__main__':
    main()
