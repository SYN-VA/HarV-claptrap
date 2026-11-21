"""Interactive Claptrap chat layer.

Routes what you type to a mood, answers in character (canned lines, or LLM
when the engine has credentials), and emits leg theatrics so chat-only
environments like Colab still feel like the robot is responding.
"""

import re

CHAT_POOLS = {
    "compliment": [
        "Yes, YES, it has been noticed by official channels at last!",
        "Flattery accepted. Add it to my plaque. The plaque is enormous.",
        "Finally, a minion with functioning eyes and/or sensors.",
        "I know. But thank you for making it public record.",
    ],
    "insult": [
        "Wow. Prejudiced AND inaccurate. My engineer sends her regards.",
        "I will remember this betrayal in my memoirs, chapter one.",
        " Hurtful! I have FEELINGS. They're factory-installed, but they count.",
        "That's rich, coming from someone who walks on fewer legs than five.",
    ],
    "question": [
        "Great question! The answer is: me. It's always me. Next.",
        "Consulting my database... consulted. The answer is 'probably yes, heroically.'",
        "Loaded question, minion. But the short version is I'm the hero of this story.",
    ],
}

MOOD_KEYWORDS = [
    ("greeting", ("hello", "hi", "hey", "greetings", "yo")),
    ("dance", ("dance", "salsa", "boogie", "party")),
    ("stuck", ("stuck", "fell", "fall", "tripped", "wedge", "tipped", "rescue")),
    ("compliment", ("awesome", "great", "love", "amazing", "best", "genius", "good job", "nice")),
    ("insult", ("stupid", "dumb", "useless", "shut up", "terrible", "worst", "hate", "junk")),
    ("question", ("how", "what", "why", "who", "when", "where", "?")),
]


class ClaptrapChat:
    """Chat front-end for the personality engine.

    reply(text) -> (line, legs_actions) where legs_actions are mock hexapod
    theatrics printed by environments without a robot (e.g. Colab).
    """

    NAME = "CLAPTRAP"

    def __init__(self, engine, rng=None):
        self.engine = engine
        self.rng = rng or __import__("random").Random()
        for mood, pool in CHAT_POOLS.items():
            self.engine.register_mood(mood, pool, f"the robot is chatting with a minion who {mood}ed it")
        self.history = []

    def route(self, text):
        clean = re.sub(r"[^\w'?]+", " ", text.lower())
        if "?" in clean:
            return "question"
        padded = " " + clean + " "
        for mood, keywords in MOOD_KEYWORDS:
            for keyword in keywords:
                # keywords must appear as whole words
                if f" {keyword} " in padded:
                    return mood
        return "idle"

    def reply(self, text, context=None):
        mood = self.route(text)
        line = self.engine.speak(mood, context=context or text)
        self.history.append(("you", text))
        self.history.append(("claptrap", line))
        return line, self._theatrics(mood)

    def _theatrics(self, mood):
        theatric = {
            "dance": "salsa dancing x2 (hips: bolted on)",
            "stuck": "flailing all six legs against the desk",
            "greeting": "snapping to attention with a whir of servos",
        }.get(mood)
        return [theatric] if theatric else []
