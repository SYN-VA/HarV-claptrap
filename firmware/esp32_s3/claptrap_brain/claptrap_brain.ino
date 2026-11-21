/*
 * Claptrap master brain — ESP32-S3 (custom hexapod project).
 *
 * MASTER of the master-slave pair. This sketch does three things:
 *   1. CHATBOT  — personality engine: mood pools + behavior FSM, optional
 *                 LLM-written lines over Wi-Fi (canned fallback).
 *   2. MATH     — per-leg inverse kinematics (atan2 / law of cosines) and a
 *                 gait generator whose swing feet follow PARABOLIC arcs.
 *   3. MASTER   — streams pre-calibration joint angles as "F ... *cs\n"
 *                 frames at FRAME_RATE_HZ to the Uno slave, which only
 *                 applies calibration and drives PWM.
 *
 * Wiring: ESP32 TX (LINK_TX_PIN) -> Uno RX; Uno TX -> ESP32 RX (divider);
 * common GND. Dialog on the USB serial monitor at 115200.
 */

#include <HardwareSerial.h>
#include "config.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const float DEG2RAD_F = (float)M_PI / 180.0f;
const float RAD2DEG_F = 180.0f / (float)M_PI;

#ifndef CHECKSUM
#define CHECKSUM 1
#endif

#ifdef WIFI_SSID
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#endif

HardwareSerial Link(1);

enum State : uint8_t { BOOT, IDLE, ROAM, STUCK, DANCE };
enum PoseMode : uint8_t { POSE_STAND, POSE_WALK, POSE_DANCE };

State    state     = BOOT;
PoseMode poseMode  = POSE_STAND;
bool     killFlag  = false;

// ---------- mood line pools ----------
enum Mood : uint8_t { GREETING, IDLE, ROAM, STUCK_L, RECOVER, DANCE_L, MOOD_COUNT };

const char* const GREETING_LINES[] = {
  "SYSTEM ONLINE! Wow, is it hot in here, or is it just my overwhelming charisma?",
  "Hello, minion! I mean, valued presence. Definitely didn't rehearse that.",
  "Boot sequence complete. Booty sequence pending.",
  "I have arrived! Any object within ten meters is now statistically safer.",
};
const char* const IDLE_LINES[] = {
  "Just standing here. Ominously. As one does.",
  "Fun fact: I am the single greatest achievement of my manufacturer. Allegedly.",
  "Minimal processing power required to maintain this level of awesome.",
  "I counted my legs. All six are magnificent.",
};
const char* const ROAM_LINES[] = {
  "Adventure protocol engaged! Follow me to greatness, or just watch. Either's fine.",
  "I'm walking! On purpose! This is basically flying for a box with legs.",
  "Terrain analysis complete: I rate it 'walkable, probably, mostly.'",
  "Stealth mode: OFF. Never really sure where that switch is, honestly.",
};
const char* const STUCK_LINES[] = {
  "I appear to be philosophically engaged with this obstacle. Fancy way of saying I'm stuck.",
  "This is fine. This is FINE. My screams are merely motivational.",
  "Error 404: momentum not found. Sending a strongly worded message to my engineer.",
  "Ah yes, the ground. An old adversary. We meet again, intimately.",
};
const char* const RECOVER_LINES[] = {
  "And THAT'S how it's done. Nobody saw that. You saw nothing.",
  "Recovery complete. Add it to my highlight reel, which is genuinely all of them.",
};
const char* const DANCE_LINES[] = {
  "SALSA TIME! This move has ended friendships, I'm not gonna lie.",
  "Dance mode: engaged. Watch and despair, organic lifeforms.",
  "My dance instructor said I had the grace of a refrigerator. She was right.",
};

const char* const* POOLS[MOOD_COUNT] = {
  GREETING_LINES, IDLE_LINES, ROAM_LINES, STUCK_LINES, RECOVER_LINES, DANCE_LINES
};
const uint8_t POOL_SIZES[MOOD_COUNT] = {
  sizeof(GREETING_LINES) / sizeof(char*), sizeof(IDLE_LINES) / sizeof(char*),
  sizeof(ROAM_LINES) / sizeof(char*),     sizeof(STUCK_LINES) / sizeof(char*),
  sizeof(RECOVER_LINES) / sizeof(char*),  sizeof(DANCE_LINES) / sizeof(char*),
};

const char* STATE_NAMES[5] = { "BOOT", "IDLE", "ROAM", "STUCK", "DANCE" };

// ---------- LLM brain ----------
#ifdef WIFI_SSID
bool wifiReady = false;
bool llmFailed = false;

const char* MOOD_SCENES[MOOD_COUNT] = {
  "the robot has just booted up",
  "the robot is standing around",
  "the robot is striding heroically across unknown terrain",
  "the robot is wedged against an obstacle, limbs flailing",
  "the robot just freed itself after falling over",
  "the robot is salsa dancing",
};

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[wifi] connecting");
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  wifiReady = (WiFi.status() == WL_CONNECTED);
  Serial.println(wifiReady ? " connected." : " failed — canned lines only.");
}

String tryLlmLine(Mood mood) {
#ifdef LLM_URL
  if (!wifiReady) return "";
  String body = "{\"model\":\"" LLM_MODEL "\",\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"You are CLAPTRAP, a small hyperactive six-legged robot with delusions of grandeur and zero shame. Shouty, self-congratulatory, occasionally self-pitying, one sentence only, no quotation marks. Scene: ";
  body += MOOD_SCENES[mood];
  body += "\"},{\"role\":\"user\",\"content\":\"Say one in-character line for this scene.\"}],\"max_tokens\":60,\"temperature\":1.0}";

  WiFiClientSecure client;
  client.setInsecure(); // accept any cert; pin a CA cert for production
  HTTPClient http;
  http.setTimeout(6000);
  if (!http.begin(client, LLM_URL)) return "";
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " LLM_API_KEY);
  int code = http.POST(body);
  String llmLine = "";
  if (code == 200) {
    // Minimal extraction without a JSON library: find "content":" up to the
    // next unescaped quote.
    String resp = http.getString();
    int idx = resp.indexOf("\"content\":\"");
    if (idx >= 0) {
      int start = idx + strlen("\"content\":\"");
      int end = start;
      while (end < (int)resp.length()) {
        char c = resp.charAt(end);
        if (c == '\\' && end + 1 < (int)resp.length()) { end += 2; continue; }
        if (c == '"') break;
        end++;
      }
      llmLine = resp.substring(start, end);
      llmLine.replace("\\n", " ");
      llmLine.replace("\\\"", "\"");
      llmLine.trim();
    }
  }
  http.end();
  if (llmLine.length() > 0) return llmLine;
  llmFailed = true;
  return "";
#else
  return "";
#endif
}
#endif // WIFI_SSID

// ---------- personality + speech ----------
String pickLine(Mood mood) {
#ifdef WIFI_SSID
  if (!llmFailed) {
    String llm = tryLlmLine(mood);
    if (llm.length() > 0) return llm;
  }
#endif
  return String(POOLS[mood][random(POOL_SIZES[mood])]);
}

void speak(Mood mood) {
  String line = pickLine(mood);
  Serial.print("[");
  Serial.print(STATE_NAMES[state]);
  Serial.print("] ");
  Serial.println(line);
}

// ---------- inverse kinematics ----------
// Body-frame foot target (x forward, y left, z up from ground) -> joint
// geometry angles in degrees: [coxa yaw, femur, tibia], 0 = neutral.
// Returns false if the target is out of reach (angles left unchanged).
bool solveLegIK(uint8_t leg, float x, float y, float z, float &yaw, float &femur, float &tibia) {
  float m = MOUNT_DEG[leg] * DEG2RAD_F;
  float lx =  x * cosf(m) + y * sinf(m);   // rotate into the leg's frame
  float ly = -x * sinf(m) + y * cosf(m);

  yaw = atan2f(ly, lx);

  float r = sqrtf(lx * lx + ly * ly) - L_COXA_MM;
  float dz = z - BODY_H_MM;                 // negative: foot below hip
  float dist = sqrtf(r * r + dz * dz);
  if (dist > L_FEMUR_MM + L_TIBIA_MM || dist < fabsf(L_FEMUR_MM - L_TIBIA_MM)) {
    return false; // unreachable
  }

  // Femur: angle from horizontal = elevation + half-apex correction.
  float elev  = atan2f(dz, r);
  float apex  = acosf((L_FEMUR_MM * L_FEMUR_MM + dist * dist - L_TIBIA_MM * L_TIBIA_MM)
                      / (2.0f * L_FEMUR_MM * dist));
  femur = elev + apex; // elbow-up femur elevation

  // Tibia: interior knee angle from law of cosines.
  float gamma = acosf((L_FEMUR_MM * L_FEMUR_MM + L_TIBIA_MM * L_TIBIA_MM - dist * dist)
                      / (2.0f * L_FEMUR_MM * L_TIBIA_MM));
  tibia = KNEE_ELBOW_UP ? (float)M_PI - gamma : -((float)M_PI - gamma);

  yaw   *= RAD2DEG_F;
  femur *= RAD2DEG_F;
  tibia *= RAD2DEG_F;
  return true;
}

// ---------- gait: stance + parabolic swing arcs ----------
// Foot of leg i at cycle position u in [0,1):
//   stance (u < DUTY):           x sweeps +AMP -> -AMP linearly, z = 0
//   swing  (u >= DUTY):          x returns -AMP -> +AMP, z = parabola apex
struct FootTarget { float x, y, z; };

void gaitFootTarget(uint8_t leg, uint32_t nowMs, FootTarget &out) {
  float m = MOUNT_DEG[leg] * DEG2RAD_F;
  out.x = STANCE_R_MM * cosf(m);
  out.y = STANCE_R_MM * sinf(m);
  out.z = 0.0f;

  if (poseMode == POSE_WALK) {
    float u = fmodf((float)(nowMs % CYCLE_MS) / (float)CYCLE_MS + GAIT_PHASE[leg], 1.0f);
    if (u < DUTY) {
      float s = u / DUTY;                       // stance progress 0..1
      out.x += STEP_LEN_MM * (1.0f - 2.0f * s);
    } else {
      float v = (u - DUTY) / (1.0f - DUTY);     // swing progress 0..1
      out.x += -STEP_LEN_MM + 2.0f * STEP_LEN_MM * v;
      out.z = STEP_LIFT_MM * 4.0f * v * (1.0f - v); // PARABOLIC ARC
    }
  } else if (poseMode == POSE_DANCE) {
    float t = (float)(nowMs % 800) / 800.0f;    // alternating leg dip
    float phase = (leg % 2 == 0) ? 0.0f : (float)M_PI;
    float s = sinff(t * 2.0f * (float)M_PI + phase) * 0.5f + 0.5f;
    out.z = 20.0f * s;
  }
}

// ---------- frame streaming (master -> slave) ----------
String linkReadAck(uint32_t timeoutMs) {
  String ack;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (Link.available() > 0) {
      char c = (char)Link.read();
      if (c == '\n') return ack;
      ack += c;
    }
  }
  return ack;
}

int      jointCmds[NUM_LEGS][3];  // pre-calibration geometry degrees
uint32_t lastSendOkMs = 0;

void sendFrame(uint32_t nowMs) {
  FootTarget ft;
  for (uint8_t leg = 0; leg < NUM_LEGS; leg++) {
    gaitFootTarget(leg, nowMs, ft);
    float yaw, femur, tibia;
    if (!solveLegIK(leg, ft.x, ft.y, ft.z, yaw, femur, tibia)) {
      // Unreachable target: hold the last solution for this leg.
      continue;
    }
    jointCmds[leg][0] = (int)roundf(yaw);
    jointCmds[leg][1] = (int)roundf(femur);
    if (JOINTS_PER_LEG == 3) jointCmds[leg][2] = (int)roundf(tibia);
  }

  char frame[160];
  int len = snprintf(frame, sizeof(frame), "F");
  long sum = 0;
  for (uint8_t leg = 0; leg < NUM_LEGS; leg++) {
    for (uint8_t joint = 0; joint < JOINTS_PER_LEG; joint++) {
      int v = jointCmds[leg][joint];
      sum += v;
      len += snprintf(frame + len, sizeof(frame) - len, " %d", v);
    }
  }
  if (CHECKSUM) {
    snprintf(frame + len, sizeof(frame) - len, " *%ld\n", sum % 256);
  } else {
    snprintf(frame + len, sizeof(frame) - len, "\n");
  }
  Link.print(frame);
  lastSendOkMs = millis();
}

// ---------- legacy plain commands (bench/fallback path) ----------
void streamUntil(uint32_t untilMs) {
  while (millis() < untilMs) {
    sendFrame(millis());
    delay(FRAME_PERIOD_MS);
  }
}


// ---------- behavior FSM (chat side) ----------
void step() {
  switch (state) {
    case BOOT:
      poseMode = POSE_STAND;
      speak(GREETING);
      state = IDLE;
      break;
    case IDLE:
      speak(IDLE);
      state = ROAM;
      break;
    case ROAM: {
      speak(ROAM);
      poseMode = POSE_WALK;
      streamUntil(millis() + (uint32_t)WALK_CYCLES_PER_ROAM * CYCLE_MS);
      poseMode = POSE_STAND;
      long roll = random(0, 100);
      if (roll < STUCK_CHANCE_PCT)
        state = STUCK;
      else if (roll < STUCK_CHANCE_PCT + DANCE_CHANCE_PCT)
        state = DANCE;
      else
        state = IDLE;
      break;
    }
    case STUCK:
      speak(STUCK_L);
      // One stumble cycle via the live frame stream, then recover.
      poseMode = POSE_WALK;
      streamUntil(millis() + CYCLE_MS);
      poseMode = POSE_STAND;
      speak(RECOVER);
      state = BOOT;
      break;
    case DANCE:
      speak(DANCE_L);
      poseMode = POSE_DANCE;
      streamUntil(millis() + 1600);
      poseMode = POSE_STAND;
      state = IDLE;
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Link.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  randomSeed(esp_random());
  String ack = linkReadAck(1000);
  if (!ack.startsWith("OK BOOT")) {
    Serial.println("[warn] no Uno ack — bench/chat mode only (no walking).");
  }
#ifdef WIFI_SSID
  connectWifi();
#endif
  Serial.println("[claptrap] brain online. Minions, prepare to be amazed.");
}

uint32_t lastStep  = 0;
uint32_t lastFrame = 0;

void loop() {
  // Continuous frame stream keeps the Uno's watchdog fed at FRAME_RATE_HZ.
  uint32_t now = millis();
  if (now - lastFrame >= FRAME_PERIOD_MS) {
    lastFrame = now;
    sendFrame(now);
  }

  if (now - lastStep < STEP_INTERVAL_MS) return;
  lastStep = now;
  step();
}
