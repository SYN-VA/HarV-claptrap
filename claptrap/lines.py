"""Canned Claptrap-flavored line library.

Original lines written in the style of a hyperactive, self-important robot
sidekick. Randomness and trigger-based selection, no network required.
"""

import random

GREETINGS = [
    "SYSTEM ONLINE! Wow, is it hot in here, or is it just my overwhelming charisma?",
    "Hello, minion! I mean — valued presence. Definitely didn't rehearse that.",
    "Boot sequence complete. Booty sequence pending.",
    "I have arrived! Any object within ten meters is now statistically safer.",
]

IDLE_QUIPS = [
    "Just standing here. Ominously. As one does.",
    "Fun fact: I am the single greatest achievement of my manufacturer. Allegedly.",
    "I once defused a bomb with my FACE. You're welcome, everyone within half a mile.",
    "Minimal processing power required to maintain this level of awesome.",
    "I counted my legs. All six are magnificent.",
    "If anyone asks, you saw me do a backflip. Official record.",
    "My sensors detect: opportunity. And also, possibly, a rock.",
]

ROAM_QUIPS = [
    "Adventure protocol engaged! Follow me to greatness, or just watch. Either's fine.",
    "I'm walking! On purpose! This is basically flying for a box with legs.",
    "Terrain analysis complete: I rate it 'walkable, probably, mostly.'",
    "Don't mind me, just redistributingAwesome across the landscape.",
    "Stealth mode: OFF. Never really sure where that switch is, honestly.",
]

STUCK_LINES = [
    "I appear to be philosophically engaged with this obstacle. It's a fancy way of saying I'm stuck.",
    "This is fine. This is FINE. My screams are merely motivational.",
    "Error 404: momentum not found. Sending a strongly worded message to my engineer.",
    "Ah yes, the ground. An old adversary. We meet again, intimately.",
    "Note to minions: when I say 'tactical repositioning', I mean I fell over.",
]

RECOVER_LINES = [
    "And THAT'S how it's done. Nobody saw that. You saw nothing.",
    "Recovery complete. Add it to my highlight reel, which is genuinely all of them.",
    "I meant to do that. It's called character development.",
]

DANCE_LINES = [
    "SALSA TIME! This move has ended friendships, I'm not gonna lie.",
    "Dance mode: engaged. Watch and despair, organic lifeforms.",
    "My dance instructor said I had the grace of a refrigerator. She was right.",
    "These hips don't lie. Mostly because they're bolted on.",
]


def pick(pool):
    return random.choice(pool)
