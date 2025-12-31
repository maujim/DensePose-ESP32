# Testing Guide for DensePose-ESP32

This document describes the testing infrastructure for the DensePose-ESP32 project.

## Overview

The project has two types of tests:

1. **Python Tests** - For the tools/utilities (load_wifi_env.py)
2. **C Unit Tests** - For the firmware modules using ESP-IDF's Unity framework

## Python Tests

### Location
```
tools/tests/
├── __init__.py
├── conftest.py          # Pytest configuration
└── test_load_wifi_env.py # Tests for load_wifi_env.py
```

### Running Python Tests

```bash
cd tools

# Install test dependencies
pip install -r requirements-dev.txt

# Run all tests
pytest

# Run with coverage
pytest --cov=. --cov-report=html

# Run specific test file
pytest tests/test_load_wifi_env.py

# Run with verbose output
pytest -v
```

### Test Coverage

The Python tests cover:

| Module | Coverage |
|--------|----------|
| load_wifi_env.py | ~90% |

#### Tests Included:

- **test_load_env_file_with_valid_entries** - Normal case: loading valid .env entries
- **test_load_env_file_with_double_quotes** - Quoted values (double quotes)
- **test_load_env_file_with_single_quotes** - Quoted values (single quotes)
- **test_load_env_file_skips_comments** - Comments (# prefix) are ignored
- **test_load_env_file_skips_empty_lines** - Empty lines are ignored
- **test_load_env_file_with_spaces_around_equals** - Whitespace handling
- **test_load_env_file_with_value_containing_equals** - Values with `=` in them
- **test_load_env_file_empty_file** - Empty file handling
- **test_load_env_file_nonexistent** - Missing file handling
- **test_load_env_file_with_whitespace_values** - Whitespace trimming
- **test_load_env_file_with_mixed_formats** - Realistic .env file
- **test_main_exits_on_missing_required_vars** - Error handling for missing vars
- **test_main_sets_environment_variables** - Environment variable setting

## Firmware Unit Tests

### Location
```
firmware/test/
├── CMakeLists.txt       # Test component build config
└── wifi_csi_test.c      # Tests for wifi_csi module
```

### Running Firmware Tests

```bash
cd firmware

# Build the test component
idf.py build

# Run tests on the ESP32
idf.py -p /dev/ttyUSB0 flash monitor

# Or run tests using the simulator (if configured)
idf.py test
```

### Test Coverage

The firmware tests verify the mathematical operations used in CSI processing without requiring WiFi hardware.

#### Tests Included:

| Test | Description |
|------|-------------|
| test_zero_iq_produces_zero_amplitude | Verifies sqrt(0² + 0²) = 0 |
| test_amplitude_pure_real_signal | Pure real: amplitude = \|I\| |
| test_amplitude_pure_imaginary_signal | Pure imaginary: amplitude = \|Q\| |
| test_amplitude_complex_signal | Pythagorean: sqrt(I² + Q²) |
| test_amplitude_max_int8_values | Edge case: max int8 values |
| test_amplitude_small_values | Precision test with small values |
| test_phase_pure_real_signal | Phase for real signals |
| test_phase_pure_imaginary_signal | Phase for imaginary signals |
| test_phase_diagonal_signal | Phase for 45° (I=Q) |
| test_csi_data_structure_size | Structure validation |
| test_csi_data_initialization | Default values |
| test_max_subcarriers_boundary | Max 64 subcarriers |
| test_min_subcarriers_boundary | Zero subcarriers |
| test_rssi_typical_range | RSSI value handling |

## Writing New Tests

### Python Tests

1. Create a new test file: `tools/tests/test_<module>.py`
2. Import the module to test
3. Write test functions with `test_` prefix
4. Run `pytest` to verify

Example:
```python
def test_my_function():
    result = my_function("input")
    assert result == "expected"
```

### Firmware Tests

1. Create a new test file or add to `firmware/test/`
2. Include Unity and relevant headers
3. Write test functions with `test_` prefix
4. Add `RUN_TEST(test_name)` in `main()`
5. Build and flash

Example:
```c
void test_my_function(void)
{
    int result = my_function(42);
    TEST_ASSERT_EQUAL(42, result);
}
```

## Continuous Integration

To add CI testing, create `.github/workflows/test.yml`:

```yaml
name: Tests
on: [push, pull_request]

jobs:
  python-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      - run: pip install -r tools/requirements-dev.txt
      - run: cd tools && pytest --cov=.

  firmware-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.0
      - run: cd firmware && idf.py build
```

## Goals

- **Coverage Target**: 80%+ for critical modules
- **Test Speed**: Unit tests should run in < 1 second
- **Reliability**: All tests must pass before merging
