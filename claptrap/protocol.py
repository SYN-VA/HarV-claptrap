"""Serial command protocol shared by the Uno firmware and desktop sim.

Master-slave split (matching the ESP32-S3 + Uno R3 hardware):

- The ESP32-S3 is the MASTER. It runs the chatbot brain and all the math
  (leg IK / trig, parabolic swing-foot arcs) and streams raw joint-angle
  frames to the Uno at FRAME_RATE_HZ.
- The Uno R3 is the SLAVE. It applies calibration (neutral offsets, invert
  flags) and writes servo PWM. It acks plain commands but stays quiet for
  frames; a watchdog drops it to a safe stand if frames stop.

Every message is one newline-terminated ASCII line.

Frames (master -> slave):
    F a0 a1 a2 ... aN *cs\n     a_i = signed geometry angles in degrees,
                                leg-major order (leg0 joint0.., leg1..),
                                0 = neutral geometry pose, before calibration.
                                cs = (sum of a_i) mod 256. Omitted when
                                CHECKSUM=0.

Plain commands (master -> slave, bench-testing / fallback):
    STAND\n
    STRIDE <steps> <period_ms>\n
    TURN <degrees>\n
    DANCE <cycles>\n
    CHIRP <count>\n

Slave -> master:
    OK <cmd>\n | ERR <reason>\n | OK BOOT\n
"""

FRAME_PREFIX = "F"
CHECKSUM_SEP = "*"


def frame(angles, checksum=True):
    body = " ".join(str(int(a)) for a in angles)
    if not checksum:
        return f"{FRAME_PREFIX} {body}\n"
    cs = sum(int(a) for a in angles) % 256
    return f"{FRAME_PREFIX} {body} {CHECKSUM_SEP}{cs}\n"


def stand():
    return "STAND\n"


def stride(steps, period_ms):
    return f"STRIDE {int(steps)} {int(period_ms)}\n"


def turn(degrees):
    return f"TURN {int(degrees)}\n"


def dance(cycles):
    return f"DANCE {int(cycles)}\n"


def chirp(count):
    return f"CHIRP {int(count)}\n"


def ok_ack(cmd):
    return f"OK {cmd}\n"
