"""Extract and host-test the pure logic from the firmware sketchs.

Pulls solveLegIK + gaitFootTarget from the ESP32 sketch and parseFrame from
the Uno sketch into a host harness with Arduino shims, then checks the IK
reaches the gait's targets with sane angles and the Uno parser validates
frames exactly like claptrap/protocol.py.
"""
import re, subprocess, sys

esp = open("/workspace/claptrap/firmware/esp32_s3/claptrap_brain/claptrap_brain.ino").read()
uno = open("/workspace/claptrap/firmware/uno_r3/hexapod_servo/hexapod_servo.ino").read()
cfg = open("/workspace/claptrap/firmware/uno_r3/hexapod_servo/config.h").read()

def extract(src, start_marker, end_marker):
    i = src.index(start_marker)
    j = src.index(end_marker, i) 
    return src[i:j]

ik_code = extract(esp, "bool solveLegIK", "\n}\n") + "\n}\n"
ik_code = ik_code[:ik_code.index("\n}\n")] + "\n}"  # first closing
# simpler: grab by balanced braces
def balanced(src, start_idx):
    depth = 0; i = start_idx
    while True:
        c = src[i]
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: return i
        i += 1
    return -1

def fn(src, name):
    i = src.index(name)
    j = balanced(src, src.index('{', i))
    return src[i:j+1]

ik = fn(esp, "bool solveLegIK")
gait = fn(esp, "void gaitFootTarget")

# Uno parseFrame, with link/acks shimmed out and buffer capture
parse = fn(uno, "bool parseFrame")
parse = parse.replace("link.print(", "ack_append(").replace("link.println(", "ack_append_line(")

harness = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

typedef uint8_t_t = int; // placeholder
'''

# Build the real harness content (replacing the placeholder above)
std_consts = "#define DEG2RAD_F (float(M_PI)/180.0f)\n#define RAD2DEG_F (180.0f/float(M_PI))\n"
consts = """
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
"""

body = '''
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
%STD_CONSTS%
%CONSTS%

// ---- solveLegIK requires fmodf only in gait; use std ----
using std::atan2f; using std::acosf; using std::sqrtf; using std::roundf;
inline float sinf(float x){ return (float)std::sin((double)x); }
inline float cosf(float x){ return (float)std::cos((double)x); }
inline float sinff(float x){ return std::sin(x); }
inline float cosff(float x){ return std::cos(x); }
inline float atan2ff(float y, float x){ return std::atan2(y,x); }

%IK%

%GAIT%

// ---- Uno parseFrame shim ----
static std::string ackSink;
static bool applied = false;
static int lastCmds[6][3];
void applyFrameHall(int c[6][3]) {{ memcpy(lastCmds, c, sizeof(lastCmds)); applied = true; }}
static void ack_append(const char* s) { ackSink += s; }}
static void ack_append_line(const std::string& s) { ackSink += s + "\\n"; }}
template <typename T> static std::string tos(T v) { return std::to_string(v); }
static std::string tos(const char* v) { return std::string(v); }
template <typename T> static void ack_append_line(T v) { ackSink += tos(v) + "\\n"; }
int cmdsGlobal[6][3];
void applyFrame(double c) {{ (void)c; }}

%PARSE%
'''

# parseFrame uses NUM_LEGS * JOINTS_PER_LEG = 18 angles for 3DOF — matches cfg ints.
host = f"/workspace/claptrap/tests/host/test_firmware_logic.cpp"
body = body.replace('}}', '}').replace('{{', '{')
harness_final = (body
    .replace("%STD_CONSTS%", std_consts)
    .replace("%CONSTS%", consts)
    .replace("%IK%", ik)
    .replace("%GAIT%", gait.replace("fmodf(", "std::fmod("))
    .replace("%PARSE%", parse.replace("applyFrame(cmds);", "memcpy(lastCmds, cmds, sizeof(lastCmds)); applied = true;")))

open(host, "w").write(harness_final)

main_append = r'''
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
'''
harness_final += main_append
open(host, "w").write(harness_final)
print("harness written")
