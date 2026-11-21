"""Run the Claptrap AI on a mock or real hexapod.

    python main.py --mock --cycles 5
    python main.py --driver rpi_servo
    python main.py --mock --llm-url https://api.mistral.ai/v1/chat/completions \
        --llm-model mistral-small-latest --llm-key $MISTRAL_API_KEY
"""

import argparse
import os
import random
import sys

from claptrap.behaviors import BehaviorEngine
from claptrap.brain import PersonalityEngine
from claptrap.drivers import MockHexapod


def build_robot(args):
    if args.driver == "mock":
        return MockHexapod()
    if args.driver == "rpi_servo":
        try:
            from claptrap.drivers.rpi_servo import RPIServoHexapod
        except ImportError:
            sys.exit("rpi_servo driver needs: pip install adafruit-circuitpython-servokit")
        return RPIServoHexapod()
    return sys.exit(f"unknown driver: {args.driver}")


def parse_args(argv=None):
    p = argparse.ArgumentParser(description="Claptrap hexapod AI")
    p.add_argument("--driver", default="mock", choices=["mock", "rpi_servo"])
    p.add_argument("--cycles", type=int, default=5, help="wake cycles (0 = run forever)")
    p.add_argument("--seed", type=int, default=None)
    p.add_argument("--stuck-chance", type=float, default=0.15)
    p.add_argument("--dance-chance", type=float, default=0.2)
    p.add_argument(
        "--llm-url",
        default=os.environ.get("CLAPTRAP_LLM_URL"),
        help="OpenAI-compatible chat completions endpoint; enables the LLM brain",
    )
    p.add_argument("--llm-model", default=os.environ.get("CLAPTRAP_LLM_MODEL"))
    p.add_argument(
        "--llm-key",
        default=os.environ.get("CLAPTRAP_LLM_KEY") or os.environ.get("MISTRAL_API_KEY"),
    )
    return p.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    brain = PersonalityEngine(
        llm_url=args.llm_url,
        llm_model=args.llm_model,
        llm_api_key=args.llm_key,
        seed=args.seed,
    )
    robot = build_robot(args)
    rng = random.Random(args.seed)
    engine = BehaviorEngine(
        robot,
        brain,
        rng=rng,
        stuck_chance=args.stuck_chance,
        dance_chance=args.dance_chance,
    )
    engine.run(cycles=args.cycles)
    if engine.kill:
        print("[claptrap] critical failure — guess I'll just lie here, dramatically.")
    else:
        print("[claptrap] standby engaged. Missing me already?")
    return 1 if engine.kill else 0


if __name__ == "__main__":
    sys.exit(main())
