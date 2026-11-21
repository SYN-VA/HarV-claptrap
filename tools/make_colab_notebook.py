#!/usr/bin/env python3
"""Generate colab/claptrap_chatbot.ipynb from the current claptrap sources.

The notebook embeds claptrap/lines.py, brain.py, behaviors.py, robot.py,
protocol.py and chatbot.py verbatim (loaded as a package via module stubs),
so it is always in sync with the repo — regenerate with:

    python tools/make_colab_notebook.py
"""

import json
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "colab" / "claptrap_chatbot.ipynb"

MODULES = ["lines", "brain", "behaviors", "robot", "protocol", "chatbot"]

INTRO_MD = """# 🤖 Claptrap Chatbot (hexapod brain, desktop mode)

The personality + behavior brain from the custom ESP32-S3 + Uno R3 hexapod,
running entirely in Colab. No robot needed: leg motion appears as `[legs]`
theatrics instead of actual servos.

**How to use:** run the cells top to bottom, then type at the `you>` prompt.
Try `hello`, `you're a genius`, `dance!`, `I fell over and I'm stuck`,
a question (ends with `?`), or an insult. Type `quit` to stop.

**Optional — LLM-fresh lines:** add a Secret in Colab named
`MISTRAL_API_KEY` (🔑 icon in the left sidebar, from
[console.mistral.ai](https://console.mistral.ai)) and restart the last cell;
otherwise Claptrap uses the offline canned-line brain.
"""

SETUP_CODE = """import json as _json
import random as _random
import sys as _sys
import types as _types
import urllib.request as _urlreq

# ---- load the embedded claptrap package ----
pkg = _types.ModuleType("claptrap")
pkg.__path__ = []
_sys.modules["claptrap"] = pkg

for name, source in EMBEDDED_MODULES.items():
    mod = _types.ModuleType("claptrap." + name)
    mod.__package__ = "claptrap"
    try:
        exec(compile(source, "<claptrap." + name + ">", "exec"), mod.__dict__)
    except ImportError:
        exec(source, mod.__dict__)
    _sys.modules["claptrap." + name] = mod
    setattr(pkg, name, mod)

import os

api_key = None
llm_url = None
try:
    from google.colab import userdata  # Colab Secrets panel
    api_key = userdata.get("MISTRAL_API_KEY")
    llm_url = "https://api.mistral.ai/v1/chat/completions"
except Exception:
    api_key = os.environ.get("MISTRAL_API_KEY")
    llm_url = "https://api.mistral.ai/v1/chat/completions" if api_key else None

from claptrap.brain import PersonalityEngine
from claptrap.chatbot import ClaptrapChat

engine = PersonalityEngine(llm_url=llm_url, llm_model="mistral-small-latest",
                           llm_api_key=api_key, seed=42)
chat = ClaptrapChat(engine)

mode = "🌐 LLM lines (Mistral)" if engine.llm_active else "📼 canned lines (offline brain)"
print(f"CLAPTRAP brain loaded — dialogue mode: {mode}")
"""

CHAT_MD = """## Chat with Claptrap

He routes your message to a mood: *greeting, compliment, insult, question,
dance, stuck,* or generic *idle quips*. Robot theatrics print as `[legs]`.
With an API key set he improvises; without it he cycles the canned pools.
"""

CHAT_CODE = """HELP = ("type 'quit' to stop · try: hello / you're a genius / dance! / "
        "I'm stuck / a question? / an insult")

def clapturn_theatrics(actions):
    for a in actions:
        print(f"  [legs] {a}")

interacted = False

try:
    print(f"CLAPTRAP> {chat.reply('hello')[0]}")
    print("   (" + HELP + ")")
    while True:
        try:
            msg = input("you> ").strip()
            interacted = True
        except (EOFError, KeyboardInterrupt):
            break
        if not msg:
            continue
        if msg.lower() in {"quit", "exit", "bye"}:
            print("CLAPTRAP> standby engaged. Missing me already?")
            break
        line, actions = chat.reply(msg)
        print(f"CLAPTRAP> {line}")
        clapturn_theatrics(actions)
except Exception as exc:
    print(f"[chat error: {exc.__class__.__name__}]")

USE_DEMO = not interacted  # demo only when there is no interactive console
DEMO_LINES = [
    "hello claptrap!",
    "you are a total genius",
    "do a dance!",
    "I fell over and now I'm stuck, help",
    "um, what do you even do all day?",
    "honestly you're a useless paperweight",
    "quit",
]
_sum = 0
if USE_DEMO:
    for msg in DEMO_LINES:
        if msg.lower() in {"quit", "exit", "bye"}:
            print("CLAPTRAP> standby engaged. Missing me already?")
            break
        line, actions = chat.reply(msg)
        print(f"you> {msg}")
        print(f"CLAPTRAP> {line}")
        clapturn_theatrics(actions)
"""


def build():
    embedded = {}
    for name in MODULES:
        src = (REPO / "claptrap" / f"{name}.py").read_text()
        embedded[name] = src

    loader_lines = []
    for name, src in embedded.items():
        loader_lines.append(f"# ---- claptrap/{name}.py ----")
        loader_lines.append(f"EMB_{name} = {json.dumps(src)}")
    loader_lines.append("EMBEDDED_MODULES = {" + ", ".join(f'"{n}": EMB_{n}' for n in MODULES) + "}")

    setup_code = "\n".join(loader_lines) + "\n\n" + SETUP_CODE

    def cell(kind, src, cid):
        c = {"cell_type": kind, "id": cid, "metadata": {},
             "source": src.splitlines(keepends=True)}
        if kind == "code":
            c["execution_count"] = None
            c["outputs"] = []
        return c

    nb = {
        "cells": [
            cell("markdown", INTRO_MD, "intro"),
            cell("code", setup_code, "setup"),
            cell("markdown", CHAT_MD, "chat-md"),
            cell("code", CHAT_CODE, "chat"),
        ],
        "metadata": {
            "kernelspec": {"display_name": "Python 3", "language": "python", "name": "python3"},
            "language_info": {"name": "python"},
            "colab": {"provenance": []},
        },
        "nbformat": 4,
        "nbformat_minor": 5,
    }

    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(nb, indent=1))
    print(f"wrote {OUT}")


if __name__ == "__main__":
    build()
