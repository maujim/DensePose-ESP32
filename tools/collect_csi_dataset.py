#!/usr/bin/env -S uv run --with pyserial --script
"""
CSI Dataset Collection Tool

Collects CSI data from ESP32 serial port for training ML models.
Supports labeling different poses/activities for supervised learning.

Usage:
    # Interactive collection mode
    python3 collect_csi_dataset.py /dev/ttyUSB0

    # Automated collection with labels
    python3 collect_csi_dataset.py /dev/ttyUSB0 --label sitting --duration 30
"""

import sys
import json
import serial
import argparse
from datetime import datetime
from pathlib import Path


class CSICollector:
    def __init__(self, port, baud=115200, output_dir='./datasets'):
        self.port = port
        self.baud = baud
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True)

        self.ser = None
        self.csi_data = []
        self.pose_data = []
        self.packet_count = 0

    def connect(self):
        """Connect to ESP32 serial port"""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            print(f"✓ Connected to {self.port} at {self.baud} baud")
            return True
        except serial.SerialException as e:
            print(f"✗ Error opening serial port: {e}")
            return False

    def disconnect(self):
        """Close serial connection"""
        if self.ser:
            self.ser.close()
            print(f"✓ Disconnected from {self.port}")

    def collect_packet(self):
        """Read and parse one CSI packet"""
        line = self.ser.readline().decode('utf-8', errors='ignore').strip()

        if not line or not line.startswith('{'):
            return None

        try:
            data = json.loads(line)
            if 'ts' in data and 'amp' in data:
                return data
        except json.JSONDecodeError:
            pass

        return None

    def collect_with_label(self, label, duration_sec, description=""):
        """Collect CSI data with a specific label"""
        print(f"\n{'='*60}")
        print(f"Collecting: {label}")
        print(f"Duration: {duration_sec} seconds")
        if description:
            print(f"Description: {description}")
        print(f"{'='*60}")
        print("Get ready... starting in 3 seconds...")
        import time
        time.sleep(3)

        start_time = time.time()
        collected = 0
        session_data = []

        print("📡 Collecting data... (Ctrl+C to stop early)")

        try:
            while time.time() - start_time < duration_sec:
                packet = self.collect_packet()
                if packet:
                    packet['label'] = label
                    packet['description'] = description
                    packet['timestamp_utc'] = datetime.utcnow().isoformat()

                    session_data.append(packet)
                    collected += 1

                    # Progress indicator
                    if collected % 10 == 0:
                        elapsed = time.time() - start_time
                        rate = collected / elapsed if elapsed > 0 else 0
                        print(f"  Packets: {collected:4d} | Rate: {rate:.1f} Hz", end='\r')

        except KeyboardInterrupt:
            print("\n⚠ Collection stopped early")

        print(f"\n✓ Collected {collected} packets for '{label}'")
        self.pose_data.extend(session_data)
        return collected

    def interactive_mode(self):
        """Interactive collection mode with user prompts"""
        poses = {
            '1': ('empty', 'Empty room - no person present'),
            '2': ('sitting', 'Person sitting still in chair'),
            '3': ('standing', 'Person standing still'),
            '4': ('walking', 'Person walking back and forth'),
            '5': ('waving', 'Person standing and waving arms'),
            '6': ('crouching', 'Person crouching down'),
        }

        print("\n" + "="*60)
        print("CSI DATASET COLLECTION - INTERACTIVE MODE")
        print("="*60)
        print("\nSelect pose to collect:")

        for key, (label, desc) in poses.items():
            print(f"  {key}. {label:15s} - {desc}")

        print("  q. Quit and save dataset")
        print()

        while True:
            choice = input("Enter choice (1-6 or q): ").strip().lower()

            if choice == 'q':
                break

            if choice in poses:
                label, description = poses[choice]
                duration = input(f"Duration in seconds (default=30): ").strip()
                try:
                    duration = int(duration) if duration else 30
                except ValueError:
                    duration = 30

                self.collect_with_label(label, duration, description)
            else:
                print("✗ Invalid choice")

    def save_dataset(self, filename=None):
        """Save collected dataset to JSON file"""
        if not self.pose_data:
            print("✗ No data to save")
            return

        if filename is None:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            filename = self.output_dir / f'csi_dataset_{timestamp}.json'

        dataset = {
            'metadata': {
                'collected_at': datetime.now().isoformat(),
                'total_packets': len(self.pose_data),
                'labels': list(set(d['label'] for d in self.pose_data))
            },
            'data': self.pose_data
        }

        with open(filename, 'w') as f:
            json.dump(dataset, f, indent=2)

        print(f"\n✓ Dataset saved to {filename}")
        print(f"  Total packets: {len(self.pose_data)}")

        # Print statistics
        label_counts = {}
        for d in self.pose_data:
            label = d['label']
            label_counts[label] = label_counts.get(label, 0) + 1

        print("\n  Packets per label:")
        for label, count in sorted(label_counts.items()):
            print(f"    {label:15s}: {count:5d} packets")


def main():
    parser = argparse.ArgumentParser(
        description='Collect CSI dataset from ESP32 for ML training',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive mode
  python3 collect_csi_dataset.py /dev/ttyUSB0

  # Collect specific pose
  python3 collect_csi_dataset.py /dev/ttyUSB0 --label sitting --duration 60

  # Multiple poses
  python3 collect_csi_dataset.py /dev/ttyUSB0 --label walking --duration 30
  python3 collect_csi_dataset.py /dev/ttyUSB0 --label sitting --duration 30 --append
        """
    )

    parser.add_argument('port', help='Serial port (e.g., /dev/ttyUSB0, COM3)')
    parser.add_argument('-b', '--baud', type=int, default=115200, help='Baud rate')
    parser.add_argument('-o', '--output-dir', default='./datasets', help='Output directory')
    parser.add_argument('-l', '--label', help='Pose label (non-interactive mode)')
    parser.add_argument('-d', '--duration', type=int, default=30, help='Collection duration (seconds)')
    parser.add_argument('--description', help='Description of the pose/activity')
    parser.add_argument('--append', action='store_true', help='Append to existing dataset')
    parser.add_argument('--dataset', help='Dataset file to append to')

    args = parser.parse_args()

    collector = CSICollector(args.port, args.baud, args.output_dir)

    if not collector.connect():
        sys.exit(1)

    try:
        # Load existing dataset if appending
        if args.append and args.dataset:
            try:
                with open(args.dataset, 'r') as f:
                    existing = json.load(f)
                    collector.pose_data = existing['data']
                print(f"✓ Loaded {len(collector.pose_data)} existing packets from {args.dataset}")
            except Exception as e:
                print(f"✗ Failed to load existing dataset: {e}")
                sys.exit(1)

        if args.label:
            # Non-interactive mode
            collector.collect_with_label(args.label, args.duration, args.description or '')
        else:
            # Interactive mode
            collector.interactive_mode()

        # Save dataset
        output_file = args.dataset if args.append else None
        collector.save_dataset(output_file)

    except KeyboardInterrupt:
        print("\n\n⚠ Interrupted by user")
    finally:
        collector.disconnect()


if __name__ == '__main__':
    main()
