/*
 * Hexapod slave servo controller — Arduino Uno R3 (custom build).
 *
 * Master: the ESP32-S3 (chatbot brain + IK + parabolic swing arcs).
 * Slave:  this Uno. It does no trig. It (1) applies servo calibration from
 * config.h and writes PWM exactly when told, and (2) provides built-in
 * bench-test moves for calibrating without the ESP32 attached.
 *
 * Messages (newline-terminated):
 *   Master -> slave:
 *     F a0 a1 ... aN *cs\n    angle frame, leg-major, signed geometry deg,
 *                             cs = sum(a_i) % 256 (if CHECKSUM)
 *     STAND / STRIDE s p / TURN d / DANCE c / CHIRP n
 *   Slave -> master:
 *     OK <cmd>\n | ERR <reason>\n | OK BOOT\n
 *
 * Watchdog: if frames stop for FRAME_TIMEOUT_MS, the Uno stands neutral and
 * ignores further frames until the stream resumes. Bench commands via USB
 * serial monitor at 115200 always work (frames are ignored after that).
 *
 * All hardware specifics live in config.h.
 */

#include <Servo.h>
#include <SoftwareSerial.h>
#include "config.h"

#if USE_PCA9685
#include <Adafruit_PWMServoDriver.h>
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
const uint16_t PWM_MIN_US = 600;
const uint16_t PWM_MAX_US = 2400;
#endif

SoftwareSerial link(LINK_RX_PIN, LINK_TX_PIN);

Servo servos[NUM_LEGS][3];

// ---- calibration + output --------------------------------------------------
void applyJoint(uint8_t leg, uint8_t joint, int geometryDeg) {
  int angle = 90 + INVERT[joint] * (geometryDeg + NEUTRAL_DEG[joint]);
  angle = constrain(angle, 0, 180);
#if USE_PCA9685
  uint16_t us = PWM_MIN_US + (uint32_t)angle * (PWM_MAX_US - PWM_MIN_US) / 180;
  pwm.writeMicroseconds(leg * 3 + joint, us);
#else
  servos[leg][joint].write(angle);
#endif
}

void applyFrame(const int cmds[NUM_LEGS][3]) {
  for (uint8_t leg = 0; leg < NUM_LEGS; leg++) {
    for (uint8_t joint = 0; joint < JOINTS_PER_LEG; joint++) {
      applyJoint(leg, joint, cmds[leg][joint]);
    }
  }
}

void standPose() {
  int cmds[NUM_LEGS][3] = {};
  applyFrame(cmds);
}

// ---- frame parsing ---------------------------------------------------------
bool parseFrame(char* line) {
  // line starts with 'F' followed by " a0 a1 ... aN [*cs]".
  const char* p = line + 1;
  int cmds[NUM_LEGS][3] = {};
  long sum = 0;
  int count = 0;
  while (*p == ' ') p++;

  while (*p && *p != '*') {
    char* end = nullptr;
    long v = strtol(p, &end, 10);
    if (end == p) return false;
    if (count >= NUM_LEGS * JOINTS_PER_LEG) return false;
    cmds[count / 3][count % 3] = (int)v;
    sum += v;
    count++;
    p = end;
    while (*p == ' ') p++;
  }

  if (count != NUM_LEGS * JOINTS_PER_LEG) {
    link.print("ERR count ");
    link.println(count);
    return false;
  }

  if (CHECKSUM) {
    if (*p != '*') {
      link.println("ERR no checksum");
      return false;
    }
    long cs = strtol(p + 1, nullptr, 10);
    if (cs != (sum % 256)) {
      link.println("ERR checksum");
      return false;
    }
  }

  applyFrame(cmds);
  return true;
}

// ---- built-in bench-test gait (no IK; for calibration / fallback) ----------
void tripodPose(const uint8_t group[3], int phaseDeg) {
  int cmds[NUM_LEGS][3] = {};
  for (uint8_t leg = 0; leg < NUM_LEGS; leg++) {
    bool swing = false;
    for (uint8_t i = 0; i < 3; i++) {
      if (group[i] == leg) { swing = true; break; }
    }
    cmds[leg][0] = swing ? phaseDeg : -phaseDeg;
    cmds[leg][1] = swing ? -FEMUR_RAISE_DEG : 0;
    if (JOINTS_PER_LEG == 3) {
      cmds[leg][2] = swing ? KNEE_BEND_DEG : 0;
    }
  }
  applyFrame(cmds);
}

bool legInGroup(uint8_t leg, const uint8_t group[3]) {
  for (uint8_t i = 0; i < 3; i++) {
    if (group[i] == leg) return true;
  }
  return false;
}

void stride(uint8_t steps, int periodMs) {
  int half = periodMs / 2;
  if (half < 50) half = 50;
  for (uint8_t s = 0; s < steps; s++) {
    tripodPose(TRIPOD_A, SWING_AMP_DEG);
    tripodPose(TRIPOD_B, -SWING_AMP_DEG);
    delay(half);
    tripodPose(TRIPOD_A, -SWING_AMP_DEG);
    tripodPose(TRIPOD_B, SWING_AMP_DEG);
    delay(half);
  }
  standPose();
}

void turn(int degrees) {
  int pulses = abs(degrees) / 10;
  if (pulses < 1) pulses = 1;
  int8_t dir = (degrees >= 0) ? 1 : -1;
  for (int p = 0; p < pulses; p++) {
    tripodPose(TRIPOD_A, SWING_AMP_DEG);
    tripodPose(TRIPOD_B, (-SWING_AMP_DEG) * dir);
    delay(100);
    tripodPose(TRIPOD_A, -SWING_AMP_DEG);
    tripodPose(TRIPOD_B, SWING_AMP_DEG * dir);
    delay(100);
  }
  standPose();
}

void dance(uint8_t cycles) {
  for (uint8_t c = 0; c < cycles; c++) {
    for (uint8_t side = 0; side < 2; side++) {
      int cmds[NUM_LEGS][3] = {};
      for (uint8_t leg = 0; leg < NUM_LEGS; leg++) {
        cmds[leg][1] = (leg % 2 == side) ? -FEMUR_RAISE_DEG : FEMUR_RAISE_DEG / 2;
      }
      applyFrame(cmds);
      delay(200);
    }
  }
  standPose();
}

void chirp(uint8_t count) {
#if BUZZER_PIN >= 0
  for (uint8_t i = 0; i < count; i++) {
    tone(BUZZER_PIN, 1200, 80);
    delay(120);
    tone(BUZZER_PIN, 1800, 80);
    delay(160);
  }
  noTone(BUZZER_PIN);
#endif
}

// ---- command dispatch ------------------------------------------------------
bool parseInt(const char* &p, long &out) {
  char* end = nullptr;
  long v = strtol(p, &end, 10);
  if (end == p) return false;
  out = v;
  p = end;
  return true;
}

void ack(bool ok, const char* cmd, const char* err = "") {
  if (ok) {
    link.print("OK ");
    link.println(cmd);
  } else {
    link.print("ERR ");
    link.print(cmd);
    if (err[0]) {
      link.print(' ');
      link.println(err);
    } else {
      link.println();
    }
  }
}

void handleCommand(char* line, bool fromUsb) {
  char* cmd = strtok(line, " ");
  if (!cmd) return;
  char rest[32] = "";
  char* tail = strtok(nullptr, "");
  if (tail) strncat(rest, tail, sizeof(rest) - 1);
  const char* p = rest;

  if (fromUsb && cmd[0] == 'F') {
    link.println("ERR frame from usb");
    return;
  }

  bool ok;
  if (strcmp(cmd, "STAND") == 0) {
    standPose();
    ok = true;
  } else if (strcmp(cmd, "STRIDE") == 0) {
    long steps = 4, period = 600;
    parseInt(p, steps);
    while (*p == ' ') p++;
    parseInt(p, period);
    stride((uint8_t)constrain(steps, 1, 50), (int)constrain(period, 100, 2000));
    ok = true;
  } else if (strcmp(cmd, "TURN") == 0) {
    long deg = 30;
    parseInt(p, deg);
    turn((int)constrain(deg, -360, 360));
    ok = true;
  } else if (strcmp(cmd, "DANCE") == 0) {
    long cycles = 2;
    parseInt(p, cycles);
    dance((uint8_t)constrain(cycles, 1, 10));
    ok = true;
  } else if (strcmp(cmd, "CHIRP") == 0) {
    long count = 1;
    parseInt(p, count);
    chirp((uint8_t)constrain(count, 1, 5));
    ok = true;
  } else {
    ack(false, cmd, "unknown");
    return;
  }

  if (fromUsb) {
    Serial.print(ok ? "OK " : "ERR ");
    Serial.println(cmd);
  } else {
    ack(ok, cmd);
  }
}

// ---- line assembly ---------------------------------------------------------
char buf[96];
uint8_t bufLen = 0;
uint32_t lastFrameMs = 0;
bool watchdogTripped = false;

void feedLine(char c, bool fromUsb) {
  if (c == '\r') return;
  if (c == '\n') {
    buf[bufLen] = '\0';
    if (bufLen > 0) {
      handleCommand(buf, fromUsb);
      if (!fromUsb && buf[0] == 'F') {
        lastFrameMs = millis();
        watchdogTripped = false;
      }
    }
    bufLen = 0;
  } else if (bufLen < sizeof(buf) - 1) {
    buf[bufLen++] = c;
  } else {
    bufLen = 0; // overflow guard: drop the line
  }
}

void checkWatchdog() {
  if (FRAME_TIMEOUT_MS == 0 || watchdogTripped) return;
  if (lastFrameMs != 0 && millis() - lastFrameMs > FRAME_TIMEOUT_MS) {
    watchdogTripped = true;
    standPose();
    link.println("ERR watchdog");
    Serial.println("[watchdog] frames lost — safe stand");
  }
}

void setup() {
  Serial.begin(115200);
  link.begin(LINK_BAUD);
#if USE_PCA9685
  pwm.begin();
  pwm.setPWMFreq(50);
#endif
  standPose();
  link.println("OK BOOT");
}

void loop() {
  checkWatchdog();
  while (link.available() > 0) {
    feedLine((char)link.read(), false);
  }
  while (Serial.available() > 0) {
    feedLine((char)Serial.read(), true);
  }
}
