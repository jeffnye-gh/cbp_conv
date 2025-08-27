import pytest
import sys
import subprocess
from pathlib import Path

pytestmark = pytest.mark.functional

def test_cli_help(cli_env):
    """Replace with your real CLI entry point; this is just a pattern."""
    exe = [sys.executable, "-m", "cbp_conv", "--help"]
    # If you don't have a -m launcher, call your console_script or module path instead.
    result = subprocess.run(exe, capture_output=True, text=True)
    assert result.returncode == 0
    assert "help" in result.stdout.lower() or result.stderr == ""

