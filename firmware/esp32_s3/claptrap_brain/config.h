// ============================================================================
// CLAPTRAP MASTER BRAIN (ESP32-S3) — EDIT THIS FILE FOR YOUR CUSTOM BUILD
// ============================================================================
// The ESP32-S3 is the MASTER: chatbot personality, leg IK (trig), parabolic
// swing-foot arcs, and frame streaming to the Uno slave. The Uno only applies
// servo calibration and PWM. Geometry below must describe YOUR mechanics;
// the calibration itself (neutral offsets, invert) lives on the Uno.
// ============================================================================

#pragma once

// ---- Serial link to the Uno slave ------------------------------------------
#define LINK_RX_PIN 18       // ESP32 pin wired to Uno TX (through divider!)
#define LINK_TX_PIN 17       // ESP32 pin wired to Uno RX (3.3V ok)
#define LINK_BAUD   57600

#define CHECKSUM      1      // must match CHECKSUM in the Uno's config.h
#define ACK_TIMEOUT_MS 400

// ---- Kinematics (millimetres, your chassis!) -------------------------------
// Leg segment lengths and hip height above the ground.
const float L_COXA_MM  = 25.0f;  // hip yaw pivot -> femur pivot
const float L_FEMUR_MM = 55.0f;  // femur pivot -> knee pivot
const float L_TIBIA_MM = 85.0f;  // knee pivot -> foot tip
const float BODY_H_MM  = 60.0f;  // hip pivot height above the ground

// Mount angle of each leg's neutral pointing direction, body frame,
// degrees, 0 = straight forward, + = counterclockwise (viewed from above).
// Typical hexapod: front pair ±45, middle pair ±90, rear pair ±135.
const float MOUNT_DEG[NUM_LEGS] = { 45, 90, 135, -45, -90, -135 };

// Resting stance: feet this far from the body center, along each leg's mount.
const float STANCE_R_MM = 70.0f;

// true if the knee bends "elbow up" (foot loads from below); false = elbow down.
const bool KNEE_ELBOW_UP = true;

// ---- Gait (parabolic swing arcs) -------------------------------------------
const float   STEP_LEN_MM  = 35.0f; // stride amplitude per leg cycle
const float   STEP_LIFT_MM = 22.0f; // swing apex height (parabola)
const uint16_t CYCLE_MS    = 800;   // full walk cycle time
const float   DUTY         = 0.5f;  // stance fraction of the cycle

// Gait phase offset per leg (0.0/0.5 = alternating tripod; matches the Uno
// TRIPOD_A/TRIPOD_B grouping: legs 0,2,4 vs 1,3,5).
const float GAIT_PHASE[6] = { 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.5f };

// ---- Frame streaming ---------------------------------------------------------
const uint8_t FRAME_RATE_HZ  = 50; // how often angle frames are sent
const uint16_t FRAME_PERIOD_MS = 1000 / FRAME_RATE_HZ; // 20 ms

// ---- Behavior timing ---------------------------------------------------------
#define STEP_INTERVAL_MS 3000  // idle wake-cycle spacing
#define WALK_CYCLES_PER_ROAM 4 // walk cycles per ROAM state

// ---- Personality tuning ------------------------------------------------------
#define STUCK_CHANCE_PCT 15
#define DANCE_CHANCE_PCT 20

// ---- Optional LLM dialogue ---------------------------------------------------
// Uncomment all to get LLM-written lines over Wi-Fi with canned fallback.
// #define WIFI_SSID   "your-ssid"
// #define WIFI_PASS   "your-pass"
// #define LLM_URL     "https://api.mistral.ai/v1/chat/completions"
// #define LLM_MODEL   "mistral-small-latest"
// #define LLM_API_KEY "your-key"
