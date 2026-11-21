import random

from claptrap.behaviors import BehaviorEngine
from claptrap.brain import PersonalityEngine
from claptrap.protocol import frame


class FakeRobot:
    def __init__(self):
        self.calls = []

    def stand(self):
        self.calls.append("stand")

    def stride(self, steps, period_ms):
        self.calls.append(("stride", steps))

    def turn(self, degrees):
        self.calls.append(("turn", degrees))

    def dance(self, cycles):
        self.calls.append(("dance", cycles))

    def speak(self, text):
        self.calls.append(("speak", text))


def make(seed=1):
    robot = FakeRobot()
    brain = PersonalityEngine(seed=seed)
    engine = BehaviorEngine(robot, brain, rng=random.Random(seed))
    return robot, engine


def test_boot_speaks_greeting_and_stands():
    robot, engine = make()
    state = engine.step()
    assert state == BehaviorEngine.IDLE
    assert engine.robot.calls[0] == "stand"
    assert any(c[0] == "speak" for c in engine.robot.calls)
    assert any(c[0] == "speak" and c[1] for c in engine.robot.calls)


def test_rule_brain_always_returns_a_line():
    brain = PersonalityEngine(seed=7)
    for mood in ("greeting", "idle", "roam", "stuck", "recover", "dance"):
        line = brain.speak(mood)
        assert isinstance(line, str) and line


def test_kill_blocks_motion():
    robot, engine = make()
    engine.kill = True
    engine.step()
    verbs = [c for c in engine.robot.calls if c != "stand" and not (c[0] == "speak")]
    assert not verbs


def test_llm_failure_falls_back_to_canned():
    brain = PersonalityEngine(llm_url="http://127.0.0.1:1/nope", llm_api_key="x", seed=3)
    line = brain.speak("idle")
    assert isinstance(line, str) and line
    assert brain.llm_active is False


def test_run_respects_cycles():
    robot, engine = make()
    engine.run(cycles=3)
    assert engine.iteration == 3


def test_frame_checksum_matches_uno_parser():
    assert frame([-30, 35], checksum=False) == "F -30 35\n"
    assert frame([-30, 35]) == "F -30 35 *5\n"
    assert frame([30, -30, 300]) == "F 30 -30 300 *44\n"
