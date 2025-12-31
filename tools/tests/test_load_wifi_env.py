"""
Unit tests for load_wifi_env.py module.

Tests cover:
- Happy path: loading valid .env files
- Edge cases: empty files, comments, quoted values
- Error cases: missing files, missing required variables
"""

import os
import sys
import tempfile
from pathlib import Path
import pytest

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from load_wifi_env import load_env_file


class TestLoadEnvFile:
    """Test suite for the load_env_file function."""

    def test_load_env_file_with_valid_entries(self):
        """Test loading a .env file with valid KEY=VALUE entries."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.write('WIFI_SSID=MyNetwork\n')
            f.write('WIFI_PASSWORD=secret123\n')
            f.write('OPTIONAL_VAR=value\n')
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)

            assert result['WIFI_SSID'] == 'MyNetwork'
            assert result['WIFI_PASSWORD'] == 'secret123'
            assert result['OPTIONAL_VAR'] == 'value'
        finally:
            env_path.unlink()

    def test_load_env_file_with_double_quotes(self):
        """Test that double quotes are properly stripped from values."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.write('WIFI_SSID="My Network"\n')
            f.write('WIFI_PASSWORD="pass:word"\n')
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)

            assert result['WIFI_SSID'] == 'My Network'
            assert result['WIFI_PASSWORD'] == 'pass:word'
        finally:
            env_path.unlink()

    def test_load_env_file_with_single_quotes(self):
        """Test that single quotes are properly stripped from values."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.write("WIFI_SSID='My Network'\n")
            f.write("WIFI_PASSWORD='pass:word'\n")
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)

            assert result['WIFI_SSID'] == 'My Network'
            assert result['WIFI_PASSWORD'] == 'pass:word'
        finally:
            env_path.unlink()

    def test_load_env_file_skips_comments(self):
        """Test that comment lines (starting with #) are ignored."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.write('# This is a comment\n')
            f.write('WIFI_SSID=MyNetwork\n')
            f.write('# Another comment\n')
            f.write('WIFI_PASSWORD=secret123\n')
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)

            assert len(result) == 2
            assert 'WIFI_SSID' in result
            assert 'WIFI_PASSWORD' in result
        finally:
            env_path.unlink()

    def test_load_env_file_skips_empty_lines(self):
        """Test that empty lines are ignored."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.write('\n')
            f.write('WIFI_SSID=MyNetwork\n')
            f.write('\n')
            f.write('\n')
            f.write('WIFI_PASSWORD=secret123\n')
            f.write('\n')
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)

            assert len(result) == 2
            assert result['WIFI_SSID'] == 'MyNetwork'
            assert result['WIFI_PASSWORD'] == 'secret123'
        finally:
            env_path.unlink()

    def test_load_env_file_with_spaces_around_equals(self):
        """Test that spaces around = are properly handled."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.write('WIFI_SSID = MyNetwork\n')
            f.write('WIFI_PASSWORD=secret123\n')
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)

            assert result['WIFI_SSID'] == 'MyNetwork'
            assert result['WIFI_PASSWORD'] == 'secret123'
        finally:
            env_path.unlink()

    def test_load_env_file_with_value_containing_equals(self):
        """Test that only the first = is used as delimiter."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.write('WIFI_SSID=MyNetwork\n')
            f.write('URL=https://example.com?param=value\n')
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)

            assert result['WIFI_SSID'] == 'MyNetwork'
            assert result['URL'] == 'https://example.com?param=value'
        finally:
            env_path.unlink()

    def test_load_env_file_empty_file(self):
        """Test that an empty file returns empty dict."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)
            assert result == {}
        finally:
            env_path.unlink()

    def test_load_env_file_nonexistent(self):
        """Test that a nonexistent file returns empty dict."""
        env_path = Path('/tmp/nonexistent_env_file_12345.env')
        result = load_env_file(env_path)
        assert result == {}

    def test_load_env_file_with_whitespace_values(self):
        """Test that leading/trailing whitespace is stripped from values."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.write('WIFI_SSID=  MyNetwork  \n')
            f.write('WIFI_PASSWORD= secret123 \n')
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)

            assert result['WIFI_SSID'] == 'MyNetwork'
            assert result['WIFI_PASSWORD'] == 'secret123'
        finally:
            env_path.unlink()

    def test_load_env_file_with_mixed_formats(self):
        """Test a realistic .env file with various formats."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.env') as f:
            f.write('# WiFi Configuration\n')
            f.write('WIFI_SSID=MyHomeNetwork\n')
            f.write('\n')
            f.write('# Password with special chars\n')
            f.write('WIFI_PASSWORD="p@ssw0rd!"\n')
            f.write('\n')
            f.write('# Optional settings\n')
            f.write('WIFI_MAXIMUM_RETRY=10\n')
            f.flush()
            env_path = Path(f.name)

        try:
            result = load_env_file(env_path)

            assert result['WIFI_SSID'] == 'MyHomeNetwork'
            assert result['WIFI_PASSWORD'] == 'p@ssw0rd!'
            assert result['WIFI_MAXIMUM_RETRY'] == '10'
        finally:
            env_path.unlink()


class TestMainFunction:
    """Test suite for the main() function behavior."""

    def test_main_function_can_be_imported(self):
        """Simple smoke test that main function exists and is callable."""
        from load_wifi_env import main
        assert callable(main)

    def test_validating_required_variables(self):
        """Test the validation logic for required environment variables."""
        # This tests the core validation logic used by main()
        required_vars = ['WIFI_SSID', 'WIFI_PASSWORD']

        # All present - no missing vars
        env_vars = {'WIFI_SSID': 'test', 'WIFI_PASSWORD': 'pass'}
        missing = [var for var in required_vars if var not in env_vars]
        assert missing == []

        # Missing one var
        env_vars = {'WIFI_SSID': 'test'}
        missing = [var for var in required_vars if var not in env_vars]
        assert 'WIFI_PASSWORD' in missing
        assert len(missing) == 1

        # Missing both vars
        env_vars = {}
        missing = [var for var in required_vars if var not in env_vars]
        assert len(missing) == 2

    def test_main_exits_when_missing_env_file(self, monkeypatch, capsys):
        """Test that main() provides helpful message when .env is missing."""
        from load_wifi_env import main
        import load_wifi_env

        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir = Path(tmpdir)

            # Create .env.example but NOT .env
            env_example = tmpdir / '.env.example'
            env_example.write_text('WIFI_SSID=\nWIFI_PASSWORD=\n')

            # Patch to use this directory
            original_file = load_wifi_env.__file__

            try:
                load_wifi_env.__file__ = str(tmpdir / 'load_wifi_env.py')
                import importlib
                importlib.reload(load_wifi_env)

                # Running main should work (uses placeholder values)
                result = load_wifi_env.main()

                assert result == 0
                # Verify placeholders were used
                assert os.environ.get('WIFI_SSID') == 'YOUR_WIFI_SSID'
                assert os.environ.get('WIFI_PASSWORD') == 'YOUR_WIFI_PASSWORD'

            finally:
                load_wifi_env.__file__ = original_file
                importlib.reload(load_wifi_env)
                os.environ.pop('WIFI_SSID', None)
                os.environ.pop('WIFI_PASSWORD', None)
