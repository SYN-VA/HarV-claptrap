"""Behavior state machine driving the hexapod."""

import time


class BehaviorEngine:
    """BOOT -> IDLE -> ROAM -> (STUCK -> BOOT | DANCE/TALK -> IDLE) loop.

    Controls the robot for one "wake cycle" per tick. `kill` stops all
    motion immediately; checked before every hardware verb.
    """

    BOOT = "BOOT"
    IDLE = "IDLE"
    ROAM = "ROAM"
    STUCK = "STUCK"
    DANCE = "DANCE"

    def __init__(self, robot, brain, rng=None, stuck_chance=0.15, dance_chance=0.2):
        self.robot = robot
        self.brain = brain
        self.rng = rng or __import__("random").Random()
        self.state = self.BOOT
        self.iteration = 0
        self.kill = False
        self.stuck_chance = stuck_chance
        self.dance_chance = dance_chance

    def step(self):
        self.iteration += 1
        if self.state == self.BOOT:
            self._safe(self.robot.stand)
            self._say("greeting")
            self.state = self.IDLE
        elif self.state == self.IDLE:
            self._say("idle")
            self.state = self.ROAM
        elif self.state == self.ROAM:
            self._say("roam")
            if self.kill:
                return self.state
            self._safe(self.robot.stride, steps=4, period_ms=600)
            if self.rng.random() < self.stuck_chance:
                self.state = self.STUCK
            elif self.rng.random() < self.dance_chance:
                self.state = self.DANCE
            else:
                self.state = self.IDLE
        elif self.state == self.STUCK:
            self._say("stuck")
            if not self.kill:
                self._safe(self.robot.stride, steps=1, period_ms=300)
                self._safe(self.robot.turn, degrees=30)
            self._say("recover")
            self.state = self.BOOT
        elif self.state == self.DANCE:
            self._say("dance")
            if not self.kill:
                self._safe(self.robot.dance, cycles=2)
            self.state = self.IDLE
        return self.state

    def run(self, cycles=3, delay_s=0.0):
        state = self.state
        for _ in range(cycles):
            if self.kill:
                break
            state = self.step()
            if delay_s > 0:
                time.sleep(delay_s)
        return state if cycles else self.state

    def _safe(self, fn, *args, **kwargs):
        if self.kill:
            return
        try:
            fn(*args, **kwargs)
        except Exception as exc:
            self.kill = True
            self._say = self._noop_say
            print(f"[KILL] hardware fault: {exc}")

    def _say(self, mood):
        line = self.brain.speak(mood)
        self._safe(self.robot.speak, line)
        if not self.kill:
            print(f"[{self.state}] {line}")

    def _noop_say(self, mood):
        pass
