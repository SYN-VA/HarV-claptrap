"""Hexapod verbs. Any driver must implement this interface."""


class Hexapod:
    """Abstract hexapod capabilities the behavior engine talks to.

    Implementations: drivers/mock.py (simulation) and
    drivers/rpi_servo.py (Raspberry Pi + PCA9685).
    """

    def stand(self):
        raise NotImplementedError

    def stride(self, steps, period_ms):
        raise NotImplementedError

    def turn(self, degrees):
        raise NotImplementedError

    def dance(self, cycles):
        raise NotImplementedError

    def speak(self, text):
        raise NotImplementedError
