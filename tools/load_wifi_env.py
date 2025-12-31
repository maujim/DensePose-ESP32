#!/usr/bin/env python3
"""
Load WiFi credentials from .env file and inject into ESP-IDF build.

This script reads the .env file (if it exists) and sets environment variables
that will be picked up by the Kconfig system during build.

Usage:
    python tools/load_wifi_env.py

Environment variables set:
    WIFI_SSID - The WiFi network SSID
    WIFI_PASSWORD - The WiFi network password
"""

import os
import sys
from pathlib import Path

def load_env_file(env_path):
    """
    Load environment variables from .env file.

    Args:
        env_path: Path to .env file

    Returns:
        dict: Environment variables as key-value pairs
    """
    env_vars = {}

    if not env_path.exists():
        return env_vars

    with open(env_path, 'r') as f:
        for line in f:
            line = line.strip()

            # Skip empty lines and comments
            if not line or line.startswith('#'):
                continue

            # Parse KEY=VALUE format
            if '=' in line:
                key, value = line.split('=', 1)
                key = key.strip()
                value = value.strip()

                # Remove quotes if present
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                elif value.startswith("'") and value.endswith("'"):
                    value = value[1:-1]

                env_vars[key] = value

    return env_vars

def main():
    # Find project root (where .env should be)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    env_file = project_root / '.env'
    env_example = project_root / '.env.example'

    # Check if .env exists
    if not env_file.exists():
        print(f"Warning: .env file not found at {env_file}", file=sys.stderr)
        print(f"Please copy {env_example} to .env and fill in your WiFi credentials", file=sys.stderr)

        # Use placeholder values if .env doesn't exist
        # This allows the build to proceed with default values
        env_vars = {
            'WIFI_SSID': 'YOUR_WIFI_SSID',
            'WIFI_PASSWORD': 'YOUR_WIFI_PASSWORD'
        }
    else:
        # Load environment variables from .env
        env_vars = load_env_file(env_file)

    # Validate required variables
    required_vars = ['WIFI_SSID', 'WIFI_PASSWORD']
    missing_vars = [var for var in required_vars if var not in env_vars]

    if missing_vars:
        print(f"Error: Missing required variables in .env: {', '.join(missing_vars)}", file=sys.stderr)
        print(f"Please check {env_example} for the required format", file=sys.stderr)
        sys.exit(1)

    # Export environment variables for the build system
    for key, value in env_vars.items():
        os.environ[key] = value
        print(f"Loaded {key}={value if key != 'WIFI_PASSWORD' else '***'}")

    return 0

if __name__ == '__main__':
    sys.exit(main())
