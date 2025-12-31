#!/usr/bin/env python3
"""
Simple CSI Data Reader

Reads CSI data from ESP32 serial port and displays it.
Useful for testing that CSI data is being received correctly.

Usage:
    python3 read_csi.py /dev/ttyUSB0
"""

import sys
import json
import serial
import argparse
from datetime import datetime


def main():
    parser = argparse.ArgumentParser(description='Read CSI data from ESP32')
    parser.add_argument('port', help='Serial port (e.g., /dev/ttyUSB0, COM3)')
    parser.add_argument('-b', '--baud', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('-v', '--verbose', action='store_true', help='Show full CSI data arrays')
    parser.add_argument('-o', '--output', help='Save CSI data to file (JSONL format)')
    args = parser.parse_args()

    # Open serial port
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
        print(f"Connected to {args.port} at {args.baud} baud")
        print("Waiting for CSI data... (Ctrl+C to exit)")
        print()
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        sys.exit(1)

    # Open output file if specified
    outfile = None
    if args.output:
        outfile = open(args.output, 'a')
        print(f"Saving data to {args.output}")
        print()

    packet_count = 0

    try:
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()

            # Skip empty lines and ESP-IDF log lines
            if not line or not line.startswith('{'):
                continue

            try:
                # Parse JSON
                data = json.loads(line)

                # Validate expected fields
                if 'ts' not in data or 'rssi' not in data or 'amp' not in data:
                    continue

                packet_count += 1

                # Display summary
                ts = data['ts']
                rssi = data['rssi']
                num = data.get('num', len(data['amp']))
                amp_mean = sum(data['amp']) / len(data['amp'])
                amp_max = max(data['amp'])

                now = datetime.now().strftime('%H:%M:%S.%f')[:-3]
                print(f"[{now}] Packet #{packet_count:4d} | "
                      f"ts={ts:8d}ms | RSSI={rssi:3d}dBm | "
                      f"subcarriers={num:2d} | "
                      f"amp: mean={amp_mean:5.1f} max={amp_max:5.1f}")

                # Show full data if verbose
                if args.verbose:
                    print(f"  Amplitude: {data['amp'][:10]}...")
                    if 'phase' in data:
                        print(f"  Phase:     {data['phase'][:10]}...")
                    print()

                # Save to file if specified
                if outfile:
                    outfile.write(json.dumps(data) + '\n')
                    outfile.flush()

            except json.JSONDecodeError:
                # Not valid JSON, skip
                pass

    except KeyboardInterrupt:
        print(f"\n\nReceived {packet_count} CSI packets")
        print("Exiting...")

    finally:
        ser.close()
        if outfile:
            outfile.close()
            print(f"Data saved to {args.output}")


if __name__ == '__main__':
    main()
