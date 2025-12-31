"""
Pytest configuration and fixtures for testing DensePose-ESP32 tools.
"""

import sys
from pathlib import Path


def pytest_configure(config):
    """Configure pytest with custom markers."""
    config.addinivalue_line("markers", "unit: Unit tests (isolated, fast)")
    config.addinivalue_line("markers", "integration: Integration tests (require external dependencies)")
    config.addinivalue_line("markers", "slow: Slow-running tests")


def pytest_collection_modifyitems(config, items):
    """Add markers to tests based on their names/paths."""
    for item in items:
        # Mark tests in test_load_wifi_env.py as unit tests
        if "load_wifi_env" in str(item.fspath):
            item.add_marker("unit")
