"""Compiles and runs the firmware-logic host harness, if a C++ toolchain exists.

The harness (test_firmware_logic.cpp) is REGENERATED from the .ino sources at
test time by generate_harness.py, so the host tests can never drift from the
firmware. It verifies the IK/gait math and the Uno frame parser without an
Arduino toolchain.
"""

import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
REPO = HERE.parent.parent


def test_firmware_logic():
    gen = subprocess.run(
        [sys.executable, str(HERE / "generate_harness.py")],
        capture_output=True, text=True,
    )
    assert gen.returncode == 0, f"harness generation failed:\n{gen.stderr}"

    cpp = HERE / "test_firmware_logic.cpp"
    assert cpp.exists()

    if shutil.which("g++") is None and shutil.which("clang++") is None:
        import pytest

        pytest.skip("no C++ compiler available")

    cc = shutil.which("g++") or shutil.which("clang++")
    out = HERE / "test_firmware_logic.bin"
    build = subprocess.run(
        [cc, "-std=c++17", "-o", str(out), str(cpp)], capture_output=True, text=True,
    )
    assert build.returncode == 0, f"host harness failed to compile:\n{build.stderr}"

    run = subprocess.run([str(out)], capture_output=True, text=True, timeout=60)
    assert run.returncode == 0, f"firmware logic failures:\n{run.stdout}\n{run.stderr}"
