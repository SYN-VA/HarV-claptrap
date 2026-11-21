"""Simulated hexapod: logs every action, safe to run anywhere."""

import sys

from ..robot import Hexapod


class MockHexapod(Hexapod):
    name = "mock"

    def stand(self):
        print("  [mock] standing: tripod gait neutral pose")

    def stride(self, steps, period_ms):
        print(f"  [mock] striding {steps} steps (period {period_ms}ms)")

    def turn(self, degrees):
        print(f"  [mock] turning {degrees} degrees")

    def dance(self, cycles):
        print(f"  [mock] salsa dancing x{cycles} (hips: bolted on)")

    def speak(self, text):
        if "--verbose" in sys.argv:
            print(f"  [mock] speaker: {text}")
