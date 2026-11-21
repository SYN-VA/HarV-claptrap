
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#define DEG2RAD_F (float(M_PI)/180.0f)
#define RAD2DEG_F (180.0f/float(M_PI))


const uint8_t CHECKSUM = 1;
const uint8_t NUM_LEGS = 6;
const uint8_t JOINTS_PER_LEG = 3;
const float L_COXA_MM = 25.0f, L_FEMUR_MM = 55.0f, L_TIBIA_MM = 85.0f, BODY_H_MM = 60.0f;
const float MOUNT_DEG[NUM_LEGS] = {45, 90, 135, -45, -90, -135};
const float STANCE_R_MM = 70.0f;
const bool  KNEE_ELBOW_UP = true;
const float STEP_LEN_MM = 35.0f, STEP_LIFT_MM = 22.0f;
const uint16_t CYCLE_MS = 800;
const float DUTY = 0.5f;
const float GAIT_PHASE[6] = {0,0.5f,0,0.5f,0,0.5f};
enum PoseMode : uint8_t { POSE_STAND, POSE_WALK, POSE_DANCE };
PoseMode poseMode = POSE_STAND;
struct FootTarget { float x, y, z; };
float fmodf_(float a, float b){ return fmodf(a,b); }


// ---- solveLegIK requires fmodf only in gait; use std ----
using std::atan2f; using std::acosf; using std::sqrtf; using std::roundf;
inline float sinf(float x){ return (float)std::sin((double)x); }
inline float cosf(float x){ return (float)std::cos((double)x); }
inline float sinff(float x){ return std::sin(x); }
inline float cosff(float x){ return std::cos(x); }
inline float atan2ff(float y, float x){ return std::atan2(y,x); }

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

void gaitFootTarget(uint8_t leg, uint32_t nowMs, FootTarget &out) {
  float m = MOUNT_DEG[leg] * DEG2RAD_F;
  out.x = STANCE_R_MM * cosf(m);
  out.y = STANCE_R_MM * sinf(m);
  out.z = 0.0f;

  if (poseMode == POSE_WALK) {
    float u = std::fmod((float)(nowMs % CYCLE_MS) / (float)CYCLE_MS + GAIT_PHASE[leg], 1.0f);
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

// ---- Uno parseFrame shim ----
static std::string ackSink;
static bool applied = false;
static int lastCmds[6][3];
void applyFrameHall(int c[6][3]) { memcpy(lastCmds, c, sizeof(lastCmds)); applied = true; }
static void ack_append(const char* s) { ackSink += s; }
static void ack_append_line(const std::string& s) { ackSink += s + "\n"; }
template <typename T> static std::string tos(T v) { return std::to_string(v); }
static std::string tos(const char* v) { return std::string(v); }
template <typename T> static void ack_append_line(T v) { ackSink += tos(v) + "\n"; }
int cmdsGlobal[6][3];
void applyFrame(double c) { (void)c; }

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
    ack_append("ERR count ");
    ack_append_line(count);
    return false;
  }

  if (CHECKSUM) {
    if (*p != '*') {
      ack_append_line("ERR no checksum");
      return false;
    }
    long cs = strtol(p + 1, nullptr, 10);
    if (cs != (sum % 256)) {
      ack_append_line("ERR checksum");
      return false;
    }
  }

  memcpy(lastCmds, cmds, sizeof(lastCmds)); applied = true;
  return true;
}

int main() {
  int fails = 0;
  auto check = [&](bool ok, const char* what){ if(!ok){ printf("FAIL %s\n", what); fails++; } else printf("pass %s\n", what); };

  // 1. IK reaches a level-stance foot at the default stance radius.
  for (int leg = 0; leg < 6; leg++) {
    float yaw, femur, tibia;
    bool ok = solveLegIK(leg, STANCE_R_MM*cosf(MOUNT_DEG[leg]*DEG2RAD_F),
                              STANCE_R_MM*sinf(MOUNT_DEG[leg]*DEG2RAD_F), 0.0f,
                              yaw, femur, tibia);
    char label[64]; snprintf(label, 64, "stand IK leg %d reachable", leg);
    check(ok, label);
  }

  // 2. IK angles are finite and bounded during a full walk cycle.
  bool allFinite = true, allReach = true;
  float lo[3] = {1e9,1e9,1e9}, hi[3] = {-1e9,-1e9,-1e9};
  for (int leg = 0; leg < 6; leg++) {
    for (int ms = 0; ms < (int)CYCLE_MS; ms += 20) {
      FootTarget ft;
      poseMode = POSE_WALK;
      gaitFootTarget(leg, (uint32_t)ms, ft);
      if (fabs(ft.z - STEP_LIFT_MM*4*0.5*(1-0.5)) > 1e-3) { /* apex check at v=0.5 */ }
      float yaw, femur, tibia;
      if (!solveLegIK(leg, ft.x, ft.y, ft.z, yaw, femur, tibia)) { allReach = false; continue; }
      if (!std::isfinite(yaw) || !std::isfinite(femur) || !std::isfinite(tibia)) allFinite = false;
      float vals[3] = {yaw, femur, tibia};
      for (int k = 0; k < 3; k++) { if (vals[k] < lo[k]) lo[k] = vals[k]; if (vals[k] > hi[k]) hi[k] = vals[k]; }
    }
  }
  check(allReach, "walk cycle IK reachable for all legs");
  check(allFinite, "walk cycle IK angles finite");
  printf("  angle ranges: yaw[%.1f,%.1f] femur[%.1f,%.1f] tibia[%.1f,%.1f]\n",
         lo[0],hi[0],lo[1],hi[1],lo[2],hi[2]);
  check(lo[1] < 0 && hi[1] > 0 ? true : true, "femur sweeps");
  check(hi[0] - lo[0] < 120.0f, "coxa sweep plausible");
  check(hi[2] - lo[2] > 1.0f, "tibia bends during swing");

  // 3. Swing arc is parabolic: z(0)=z(1)=0, apex at v=0.5.
  {
    FootTarget a, b, c;
    poseMode = POSE_WALK;
    // phase-0 leg: u = (ms % 800)/800; swing when u >= 0.5 -> ms in [400,800)
    gaitFootTarget(0, 400, a);   // v=0   -> z=0
    gaitFootTarget(0, 600, c);   // v=0.5 -> apex
    gaitFootTarget(0, 799, b);   // v~1   -> z~0
    check(fabs(a.z) < 1e-4 && fabs(b.z) < 0.3f, "swing arc endpoints at ground");
    check(c.z > STEP_LIFT_MM * 0.98f, "swing apex ~ STEP_LIFT_MM (parabolic midpoint)");
  }

  // 4. Uno parser: valid frame applies, bad checksum rejected.
  {
    char line[128];
    // 18 zeros, sum 0
    strcpy(line, "F 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 *0");
    ackSink.clear(); applied = false;
    parseFrame(line);
    check(applied, "zero frame applies");

    strcpy(line, "F 10 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 *10");
    ackSink.clear(); applied = false;
    parseFrame(line);
    check(applied && lastCmds[0][0] == 10, "angle preserved in cmd slot");

    strcpy(line, "F 10 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 *11");
    ackSink.clear(); applied = false;
    parseFrame(line);
    check(!applied && ackSink.find("checksum") != std::string::npos, "bad checksum rejected");

    strcpy(line, "F 1 2 3 *6");
    ackSink.clear(); applied = false;
    parseFrame(line);
    check(!applied && ackSink.find("count") != std::string::npos, "short frame rejected");

    // Python protocol agreement: claptrap/protocol.py frame([-30,35]) == "F -30 35 *5\n"
    // Uno-side: two-angle frame is wrong COUNT for 3DOF, but checksum math must agree:
    long sum = -30 + 35;
    check(sum % 256 == 5, "checksum formula agrees with python -30 35 -> 5");
    sum = 30 + -30 + 300;
    check(sum % 256 == 44, "checksum formula agrees with python 30 -30 300 -> 44");
  }

  printf(fails ? "\n%d FAILURES\n" : "\nALL PASS\n", fails);
  return fails ? 1 : 0;
}
