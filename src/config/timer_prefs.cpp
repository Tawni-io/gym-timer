#include "config/timer_prefs.h"

#include <Preferences.h>

namespace {

Preferences g_prefs;
constexpr const char* kNs = "gymtimer";
constexpr const char* kKeyDur = "dur_sec";
constexpr const char* kKeySets = "wo_sets";
constexpr const char* kKeyWork = "wo_work";
constexpr const char* kKeyRest = "wo_rest";
constexpr const char* kKeyWoInit = "wo_init";
constexpr const char* kKeyFace = "face";
constexpr const char* kKeySMode = "s_mode";
constexpr const char* kKeyFlip = "flip";
constexpr uint32_t kDefaultSec = 30;

bool prefs_begin_rw(void) {
  return g_prefs.begin(kNs, false);
}

}  // namespace

TimerPrefs timer_prefs_load(void) {
  TimerPrefs p = {};
  p.countdown_sec = kDefaultSec;
  p.workout = timer_workout_default();
  p.workout_ready = false;
  p.workout_face = false;
  p.simple_mode = kModeStopwatch;
  p.flip = 0;

  if (!g_prefs.begin(kNs, true)) {
    return p;
  }

  p.countdown_sec = g_prefs.getUInt(kKeyDur, kDefaultSec);
  WorkoutProfile wo = timer_workout_default();
  wo.sets = (uint8_t)g_prefs.getUChar(kKeySets, wo.sets);
  wo.work_sec = (uint16_t)g_prefs.getUShort(kKeyWork, wo.work_sec);
  wo.rest_sec = (uint16_t)g_prefs.getUShort(kKeyRest, wo.rest_sec);
  p.workout = timer_workout_clamp(wo);
  p.workout_face = g_prefs.getUChar(kKeyFace, 0) != 0;
  if (g_prefs.isKey(kKeyWoInit)) {
    p.workout_ready = g_prefs.getUChar(kKeyWoInit, 0) != 0;
  } else {
    p.workout_ready = p.workout_face;
  }
  p.simple_mode = (g_prefs.getUChar(kKeySMode, 0) == 1) ? kModeCountdown : kModeStopwatch;
  p.flip = g_prefs.getUChar(kKeyFlip, 0) ? 1 : 0;
  g_prefs.end();

  if (p.countdown_sec > timer_countdown_max_sec()) {
    p.countdown_sec = kDefaultSec;
  }
  if (p.countdown_sec != 0 && (p.countdown_sec % timer_adjust_step_sec()) != 0) {
    p.countdown_sec = kDefaultSec;
  }
  return p;
}

void timer_prefs_save(const TimerPrefs& p) {
  if (!prefs_begin_rw()) return;
  const WorkoutProfile wo = timer_workout_clamp(p.workout);
  uint32_t sec = p.countdown_sec;
  if (sec > timer_countdown_max_sec()) sec = timer_countdown_max_sec();
  g_prefs.putUInt(kKeyDur, sec);
  g_prefs.putUChar(kKeySets, wo.sets);
  g_prefs.putUShort(kKeyWork, wo.work_sec);
  g_prefs.putUShort(kKeyRest, wo.rest_sec);
  g_prefs.putUChar(kKeyWoInit, p.workout_ready ? 1 : 0);
  g_prefs.putUChar(kKeyFace, (p.workout_face && p.workout_ready) ? 1 : 0);
  g_prefs.putUChar(kKeySMode, p.simple_mode == kModeCountdown ? 1 : 0);
  g_prefs.putUChar(kKeyFlip, p.flip ? 1 : 0);
  g_prefs.end();
}

uint32_t timer_prefs_load_duration_sec(void) {
  return timer_prefs_load().countdown_sec;
}

void timer_prefs_save_duration_sec(uint32_t sec) {
  if (sec > timer_countdown_max_sec()) {
    sec = timer_countdown_max_sec();
  }
  if (!prefs_begin_rw()) return;
  g_prefs.putUInt(kKeyDur, sec);
  g_prefs.end();
}

void timer_prefs_save_workout(const WorkoutProfile& p) {
  const WorkoutProfile wo = timer_workout_clamp(p);
  if (!prefs_begin_rw()) return;
  g_prefs.putUChar(kKeySets, wo.sets);
  g_prefs.putUShort(kKeyWork, wo.work_sec);
  g_prefs.putUShort(kKeyRest, wo.rest_sec);
  g_prefs.end();
}

void timer_prefs_save_workout_ready(bool ready) {
  if (!prefs_begin_rw()) return;
  g_prefs.putUChar(kKeyWoInit, ready ? 1 : 0);
  g_prefs.end();
}

void timer_prefs_save_face(bool workout_face, TimerMode simple_mode) {
  if (!prefs_begin_rw()) return;
  g_prefs.putUChar(kKeyFace, workout_face ? 1 : 0);
  g_prefs.putUChar(kKeySMode, simple_mode == kModeCountdown ? 1 : 0);
  g_prefs.end();
}

void timer_prefs_save_flip(uint8_t flip) {
  if (!prefs_begin_rw()) return;
  g_prefs.putUChar(kKeyFlip, flip ? 1 : 0);
  g_prefs.end();
}
