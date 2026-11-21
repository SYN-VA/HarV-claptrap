// ============================================================================
// HEXAPOD SLAVE CONTROLLER (Uno R3) — EDIT THIS FILE TO MATCH YOUR BUILD
// ============================================================================
// The Uno is the lower half of a master-slave system: the ESP32-S3 runs the
// chatbot brain and ALL the math (leg IK, parabolic swing arcs) and streams
// pre-calibration joint angles; this sketch applies calibration and writes
// servo PWM. It should never need gait code changes.
//
// Calibration lives here (NEUTRAL_DEG, INVERT) because it is a property of
// how the servos are mounted, not of the robot's brain. Calibrate BEFORE the
// first walk: SERVO_PWR off, set each joint to neutral, remount horns.
// ============================================================================

#pragma once

// ---- Serial link to the ESP32-S3 master ------------------------------------
// Uno RX <- ESP32 TX is fine at 3.3V.
// Uno TX -> ESP32 RX is 5V: USE A VOLTAGE DIVIDER (e.g. 10k/5k), or leave the
// Uno TX unconnected (frames are one-way) and rely on the serial monitor.
const uint8_t LINK_RX_PIN = 7;    // Uno pin wired to ESP32-S3 TX (GPIO17)
const uint8_t LINK_TX_PIN = 8;    // Uno pin wired to ESP32-S3 RX (divided!)
const long   LINK_BAUD   = 57600; // high enough for 50 Hz angle frames

// ---- Slave watchdog ---------------------------------------------------------
// If no frame arrives within this window, drop to a safe neutral stand.
// 0 disables the watchdog (bench testing without the ESP32).
const uint16_t FRAME_TIMEOUT_MS = 500;

// Reject frames whose checksum trailer does not match (parity with
// claptrap/protocol.py frame()). Recommended on a robot with motor noise.
const uint8_t CHECKSUM = 1;

// ---- Robot geometry ---------------------------------------------------------
const uint8_t NUM_LEGS       = 6;
const uint8_t JOINTS_PER_LEG = 2;   // 2 = hip(yaw)+lift,
                                    // 3 = coxa(yaw)+femur(lift)+tibia(knee)
const uint8_t USE_PCA9685    = 0;   // 1 = PCA9685 hat at 0x40; channel =
                                    // leg*3 + joint. One PCA9685 = 16ch, so
                                    // 2DOF legs (12ch) fit; 3DOF (18ch)
                                    // needs a second board at 0x41.

// Servo pin (or PCA9685 channel) per [leg][joint].
// Columns: joint0 = hip/coxa (yaw), joint1 = lift/femur, joint2 = knee/tibia
// (third column ignored when JOINTS_PER_LEG == 2).
// *** REPLACE WITH YOUR WIRING ***
const uint8_t SERVO_PINS[6][3] = {
  { 2,  3, 22},   // leg 0: front-left
  { 4,  5, 23},   // leg 1: mid-left
  { 6,  7, 24},   // leg 2: rear-left
  { 9, 10, 25},   // leg 3: front-right
  {11, 12, 26},   // leg 4: mid-right
  {13, 14, 27},   // leg 5: rear-right
};

// Which legs swing together in the built-in bench-test tripod gait and the
// ESP32's default gait phase offsets (0.0 / 0.5 of the walk cycle).
const uint8_t TRIPOD_A[3] = {0, 2, 4};
const uint8_t TRIPOD_B[3] = {1, 3, 5};

// ---- Calibration ------------------------------------------------------------
// Servo angle = 90 + INVERT[j] * (frame_angle + NEUTRAL_DEG[j]).
// NEUTRAL_DEG: offset added to the master's geometry zero for each joint.
const int8_t NEUTRAL_DEG[3] = {0, 35, -20};

// -1 flips a joint direction (mirror-mounted servos).
const int8_t INVERT[3] = {1, 1, 1};

// Built-in bench gait amplitudes (no IK on the Uno; ESP32 does that).
const int8_t SWING_AMP_DEG   = 20;
const int8_t FEMUR_RAISE_DEG = 25;
const int8_t KNEE_BEND_DEG   = 15;

// ---- Optional sound ---------------------------------------------------------
const int8_t BUZZER_PIN = -1;    // passive buzzer pin, e.g. A3; -1 disables
