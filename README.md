# Claptrap AI for a custom hexapod (ESP32-S3 + Uno R3)

A Claptrap-inspired AI for a custom-built hexapod robot: big personality,
questionable competence, endless enthusiasm — proving that true heroism comes
in small, understaffed boxes.

## Hardware split (master / slave)

- **ESP32-S3 = master.** Runs the chatbot brain (mood line pools, behavior
  state machine, optional LLM via Wi-Fi) AND all the math: per-leg inverse
  kinematics (atan2 / law of cosines) and a gait generator whose swing feet
  ride **parabolic arcs**. Streams 50 Hz joint-angle frames to the Uno.
- **Uno R3 = slave.** Applies servo calibration and drives PWM. No gait code,
  no trig — plus a 500 ms frame watchdog that drops it to a safe stand if the
  master dies mid-sentence.

## Layout

```
claptrap/                       Python reference brain (mirrors the ESP32 logic)
  brain.py                      PersonalityEngine: canned lines + optional LLM
  lines.py                      Line library
  behaviors.py                  Behavior state machine
  chatbot.py                    Interactive chat layer (used by the Colab notebook)
  robot.py, protocol.py         Hexapod verbs + master/slave serial protocol
  drivers/                      MockHexapod (sim) — real drive is the firmware
firmware/
  esp32_s3/claptrap_brain/      MASTER: chatbot + IK + parabolic arcs + frames
  uno_r3/hexapod_servo/         SLAVE: calibration + PWM + watchdog + bench moves
  README.md                     Wiring, bring-up order, calibration
colab/
  claptrap_chatbot.ipynb        Generated: chat with Claptrap on Google Colab
tools/
  make_colab_notebook.py        Regenerates the notebook from claptrap/ sources
main.py                         Run the Python brain on the desktop
tests/
  test_brain.py                 Brain/protocol unit tests
  host/                         Compiles the firmware's pure logic on your PC
```

Every hardware-specific value lives in a single `config.h` per board —
custom-build friendly by design.

## Quick start

```bash
python main.py                   # simulated Claptrap, prints its life story
python main.py --cycles 10 --seed 42
python -m pytest tests/ -q       # brain tests + host-compiled firmware logic
```

Google Colab: open `colab/claptrap_chatbot.ipynb` (upload it from this repo,
or File → Upload notebook) and run the two cells — it embeds the same brain,
does an interactive chat REPL, and shows leg theatrics as `[legs]` prints.
Drop a Colab Secret named `MISTRAL_API_KEY` for LLM-improvised lines.

## Behavior state machine

`BOOT → IDLE → ROAM → (STUCK → BOOT, or DANCE → IDLE)`

- IDLE: chatty quips.
- ROAM: IK-driven walk (swing feet on parabolic arcs), may randomly trip into
  STUCK and recover with total denial, or break into salsa.
- Everything, including STUCK and DANCE, is narrated loudly.

## LLM dialogue (optional, ESP32 side)

Uncomment the `WIFI_*` / `LLM_*` block in `firmware/esp32_s3/claptrap_brain/config.h`
(Mistral or any OpenAI-compatible endpoint). Any network failure falls back to
the canned lines — the robot never goes mute, and walking never depends on Wi-Fi.

The desktop Python brain accepts the same flags: `--llm-url`, `--llm-model`,
`--llm-key`.

## Robo-taming notes

- The line library is original Claptrap-*flavored* writing, not verbatim
  Borderlands audio (keeps you clear of Gearbox's voice lines).
- `STUCK` is scripted randomness for now — current sensing or IMU tilt on the
  ESP32 would make the denial earned rather than theatrical.
