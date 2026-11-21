"""Personality engine: rule-based lines with an optional LLM upgrade."""

import json
import random
import urllib.request

from . import lines


class PersonalityEngine:
    """Decides what the robot says.

    Rule mode picks from canned pools. LLM mode asks an OpenAI-compatible
    chat completions endpoint to write the line; any failure falls back to
    the canned pools so the robot never goes mute mid-skit. New moods can be
    registered at runtime (e.g. the chat layer adds compliment/insult).
    """

    def __init__(self, llm_url=None, llm_model=None, llm_api_key=None, seed=None):
        self.llm_url = llm_url
        self.llm_model = llm_model
        self.llm_api_key = llm_api_key
        self._rng = random.Random(seed)
        self.scenes = {
            "greeting": "the robot has just booted up",
            "idle": "the robot is standing around",
            "roam": "the robot is striding heroically across unknown terrain",
            "stuck": "the robot is wedged against an obstacle, limbs flailing",
            "recover": "the robot just freed itself after falling over",
            "dance": "the robot is salsa dancing",
        }
        self._pools = {
            "greeting": lines.GREETINGS,
            "idle": lines.IDLE_QUIPS,
            "roam": lines.ROAM_QUIPS,
            "stuck": lines.STUCK_LINES,
            "recover": lines.RECOVER_LINES,
            "dance": lines.DANCE_LINES,
        }
        self._llm_failed = False

    def register_mood(self, mood, pool, scene):
        self._pools[mood] = list(pool)
        self.scenes[mood] = scene

    @property
    def llm_active(self):
        return bool(self.llm_url) and not self._llm_failed

    def speak(self, mood, context=None):
        if self.llm_active:
            text = self._llm_line(mood, context)
            if text:
                return text
            self._llm_failed = True
        return self._rng.choice(self._pools[mood])

    def _llm_line(self, mood, context=None):
        try:
            scene = self.scenes[mood]
            user_content = (
                context
                or "Say one in-character line for this scene."
            )
            body = json.dumps({
                "model": self.llm_model or "mistral-small-latest",
                "messages": [
                    {
                        "role": "system",
                        "content": (
                            "You are CLAPTRAP, a small hyperactive six-legged "
                            "robot with delusions of grandeur and zero shame. "
                            "Shouty, self-congratulatory, occasionally self-pitying, "
                            "one sentence only, no quotation marks. Scene: "
                            + scene
                            + " The user just said to the robot: respond in character."
                        ),
                    },
                    {"role": "user", "content": user_content},
                ],
                "max_tokens": 60,
                "temperature": 1.0,
            }).encode()
            req = urllib.request.Request(
                self.llm_url,
                data=body,
                headers={
                    "Content-Type": "application/json",
                    "Authorization": "Bearer " + (self.llm_api_key or ""),
                },
            )
            with urllib.request.urlopen(req, timeout=6) as resp:
                data = json.loads(resp.read())
            return data["choices"][0]["message"]["content"].strip().strip('"')
        except Exception:
            return None
