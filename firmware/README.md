# Firmware — ESP32-S3 (master) + Uno R3 (slave)

## Architecture

```
            +---------------------------------+          +---------------------------+
            | ESP32-S3  — CLAPTRAP MASTER     |  UART    | Uno R3 — SERVO SLAVE      |
            |  * chatbot personality + FSM    | -------> |  * applies calibration    |
            |  * leg IK (trig)                |  57600   |    (neutral, invert)      |
            |  * parabolic swing-foot arcs    |  frames  |  * writes servo PWM       |
            |  * streams joint-angle frames   |          |  * 500 ms frame watchdog  |
            +---------------------------------+          +---------------------------+
```

The ESP32 does ALL the math. Every 20 ms (50 Hz) it solves IK for the foot
targets and streams one frame of pre-calibration joint angles, leg-major:
`F a0 a1 ... aN *cs\n` (numbers = signed geometry degrees, 0 = neutral;
`cs = sum(angles) % 256`). The Uno never computes a gait — it adds its
calibration (`90 + INVERT * (angle + NEUTRAL)`) and drives PWM, and drops to
a safe neutral stand if frames stop for 500 ms.

The chatbot brain (mood line pools, behavior state machine, optional LLM via
Wi-Fi) lives entirely on the ESP32. Dialogue goes to the USB serial monitor
at 115200.

Both sketches also keep non-IK bench moves (`STAND`, `STRIDE s p`, `TURN d`,
`DANCE c`, `CHIRP n`) — type them into the Uno's USB serial monitor to test
servos with the ESP32 unplugged.

## Wiring

| ESP32-S3       | Uno R3                                                          |
|----------------|-----------------------------------------------------------------|
| GPIO17 (TX)    | pin 7 (RX via SoftwareSerial)                                   |
| RX (set it)    | pin 8 — **insert a 5V→3.3V divider** (Uno TX is 5V; the ESP32-S3 is not 5V tolerant) |
| GND            | GND (common ground is mandatory)                                |

Servo power comes from your own BEC/buck, never the Uno's 5V rail.

## Bring-up order

1. **Uno alone**: set `SERVO_PINS`, `NUM_LEGS`, `JOINTS_PER_LEG`, `USE_PCA9685`
   in `uno_r3/hexapod_servo/config.h`. Flash, open the USB monitor (115200),
   and send `STAND`. All legs should hold the neutral pose. Then
   `STRIDE 2 600` with the robot on a stand.
2. **Calibrate**: with servo power off, set each servo to its horn-neutral,
   then adjust `NEUTRAL_DEG[j]` / `INVERT[j]` in the Uno `config.h` until the
   neutral pose matches your build mechanically. Verify with `STAND`.
3. **ESP32**: set `LINK_*` pins, then the kinematics block
   (`L_COXA_MM`/`L_FEMUR_MM`/`L_TIBIA_MM`/`BODY_H_MM`, `MOUNT_DEG`,
   `STANCE_R_MM`) in `esp32_s3/claptrap_brain/config.h` to your chassis
   measurements. Flash. It prints a no-ack warning if the Uno is missing
   (chat-only mode).
4. **First walk**: `WALK_CYCLES_PER_ROAM` starts at 4 cycles; keep the robot
   on a stand until the streamed angles look right, then try the floor.
   Reduce `STEP_LEN_MM`, `STEP_LIFT_MM`, `CYCLE_MS` if it's wobbly.

## Host-side verification (no Arduino toolchain needed)

The pure math and parsers are extracted from the .ino files and tested on
your PC, so host tests can never drift from the firmware:

```bash
python -m pytest tests/ -q
```

The suite regenerates and compiles a host harness from the ESP32's
`solveLegIK`/`gaitFootTarget` and the Uno's `parseFrame` and checks that the
IK reaches every gait target with finite angles, that swing feet ride
parabolic arcs, and that the Uno's frame parser matches
`claptrap/protocol.py` exactly.

## Optional: LLM dialogue

Uncomment the `WIFI_*` / `LLM_*` block in the ESP32 `config.h` (Mistral API or
any OpenAI-compatible endpoint). Lines are LLM-written per mood; every
failure path falls back to the canned pools, and the walk/pose engine never
depends on the network.

## Notes & limits

- One PCA9685 = 16 channels: 2DOF legs (12 ch) fit; 3DOF (18 ch) needs a
  second board — address it at 0x41 and extend the channel mapping.
- `STUCK` is currently scripted randomness, not real detection — a good next
  step is current sensing or accelerometer tilt on the ESP32.
- The ESP32 uses `setInsecure()` for HTTPS; pin a CA cert before exposing the
  key on a network you don't own.
