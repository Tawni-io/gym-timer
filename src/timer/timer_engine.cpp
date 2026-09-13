#include "timer/timer_engine.h"

#include <stdio.h>
#include <string.h>

namespace {

constexpr uint32_t kStepSec = 30;
constexpr uint32_t kMaxSec = 30 * 60;  // 30:00
constexpr uint32_t kDefaultSec = 30;

struct ModeState {
  TimerPhase phase;
  uint32_t value_ms;  // elapsed or remaining
  uint32_t mark_ms;   // millis at last tick while running
};

TimerMode g_mode = kModeStopwatch;
TimerMode g_simple_mode = kModeStopwatch;
ModeState g_sw = {};
ModeState g_cd = {};
ModeState g_wo = {};
uint32_t g_cd_duration_sec = kDefaultSec;
WorkoutProfile g_wo_prof = {5, 120, 30};
uint8_t g_wo_set = 1;
bool g_wo_resting = false;
bool g_workout_ready = false;

ModeState& active(void) {
  if (g_mode == kModeStopwatch) return g_sw;
  if (g_mode == kModeCountdown) return g_cd;
  return g_wo;
}

uint32_t clamp_duration_sec(uint32_t sec) {
  if (sec > kMaxSec) return kMaxSec;
  const uint32_t steps = (sec + kStepSec / 2) / kStepSec;
  uint32_t snapped = steps * kStepSec;
  if (snapped > kMaxSec) snapped = kMaxSec;
  return snapped;
}

void load_countdown_duration(uint32_t sec) {
  g_cd_duration_sec = clamp_duration_sec(sec);
  g_cd.value_ms = g_cd_duration_sec * 1000u;
  g_cd.phase = kPhaseIdle;
  g_cd.mark_ms = 0;
}

void pause_if_running(ModeState& s) {
  if (s.phase == kPhaseRunning) {
    s.phase = kPhasePaused;
    s.mark_ms = 0;
  }
}

void load_work_leg(void) {
  g_wo_resting = false;
  g_wo.value_ms = (uint32_t)g_wo_prof.work_sec * 1000u;
  g_wo.phase = kPhaseIdle;
  g_wo.mark_ms = 0;
}

void load_rest_leg(void) {
  g_wo_resting = true;
  g_wo.value_ms = (uint32_t)g_wo_prof.rest_sec * 1000u;
  g_wo.phase = kPhaseIdle;
  g_wo.mark_ms = 0;
}

void reset_workout_idle(void) {
  g_wo_set = 1;
  load_work_leg();
}

void start_leg_running(void) {
  g_wo.phase = kPhaseRunning;
  g_wo.mark_ms = 0;
}

void advance_workout_leg(void) {
  if (g_wo_resting) {
    if (g_wo_set < g_wo_prof.sets) {
      g_wo_set++;
    }
    load_work_leg();
    start_leg_running();
    return;
  }

  if (g_wo_set >= g_wo_prof.sets) {
    g_wo.value_ms = 0;
    g_wo.phase = kPhaseDone;
    g_wo.mark_ms = 0;
    g_wo_resting = false;
    return;
  }

  if (g_wo_prof.rest_sec == 0) {
    g_wo_set++;
    load_work_leg();
    start_leg_running();
    return;
  }

  load_rest_leg();
  start_leg_running();
}

}  // namespace

WorkoutProfile timer_workout_default(void) {
  WorkoutProfile p = {5, 120, 30};
  return p;
}

WorkoutProfile timer_workout_clamp(WorkoutProfile p) {
  if (p.sets < kWorkoutSetsMin) p.sets = kWorkoutSetsMin;
  if (p.sets > kWorkoutSetsMax) p.sets = kWorkoutSetsMax;
  if (p.work_sec < kWorkoutWorkMinSec) p.work_sec = kWorkoutWorkMinSec;
  if (p.work_sec > kWorkoutWorkMaxSec) p.work_sec = kWorkoutWorkMaxSec;
  if (p.rest_sec > kWorkoutRestMaxSec) p.rest_sec = kWorkoutRestMaxSec;
  return p;
}

void timer_workout_format_line(char* buf, size_t n, const WorkoutProfile& p) {
  if (!buf || n == 0) return;
  const WorkoutProfile c = timer_workout_clamp(p);
  snprintf(buf, n, "%ux%u:%02u / %u:%02u", (unsigned)c.sets, (unsigned)(c.work_sec / 60),
           (unsigned)(c.work_sec % 60), (unsigned)(c.rest_sec / 60),
           (unsigned)(c.rest_sec % 60));
}

uint32_t timer_adjust_step_sec(void) {
  return kStepSec;
}

uint32_t timer_countdown_max_sec(void) {
  return kMaxSec;
}

void timer_engine_init(uint32_t countdown_duration_sec, const WorkoutProfile& workout,
                       bool workout_face, TimerMode simple_mode, bool workout_ready) {
  g_simple_mode = (simple_mode == kModeCountdown) ? kModeCountdown : kModeStopwatch;
  g_sw = {};
  g_sw.phase = kPhaseIdle;
  g_sw.value_ms = 0;
  g_sw.mark_ms = 0;
  g_cd = {};
  load_countdown_duration(countdown_duration_sec == 0 ? kDefaultSec : countdown_duration_sec);
  g_wo_prof = timer_workout_clamp(workout);
  reset_workout_idle();
  g_workout_ready = workout_ready;
  g_mode = workout_face ? kModeWorkout : g_simple_mode;
}

void timer_engine_tick(uint32_t now_ms) {
  ModeState& s = active();
  if (s.phase != kPhaseRunning) {
    return;
  }

  if (s.mark_ms == 0) {
    s.mark_ms = now_ms;
    return;
  }

  const uint32_t dt = now_ms - s.mark_ms;
  s.mark_ms = now_ms;

  if (g_mode == kModeStopwatch) {
    constexpr uint32_t kMaxMs = (99u * 60u + 59u) * 1000u + 990u;
    if (s.value_ms < kMaxMs) {
      const uint32_t room = kMaxMs - s.value_ms;
      s.value_ms += (dt < room) ? dt : room;
    }
    return;
  }

  if (dt >= s.value_ms) {
    s.value_ms = 0;
    s.mark_ms = 0;
    if (g_mode == kModeWorkout) {
      advance_workout_leg();
    } else {
      s.phase = kPhaseDone;
    }
    return;
  }
  s.value_ms -= dt;
}

TimerSnapshot timer_engine_snapshot(void) {
  const ModeState& s = active();
  TimerSnapshot snap = {};
  snap.mode = g_mode;
  snap.phase = s.phase;
  snap.display_ms = s.value_ms;
  snap.duration_sec = g_cd_duration_sec;
  snap.warning_last10 = (g_mode != kModeStopwatch && s.phase == kPhaseRunning && s.value_ms <= 10000u);
  snap.workout_set = g_wo_set;
  snap.workout_sets = g_wo_prof.sets;
  snap.workout_resting = g_wo_resting;
  snap.workout_ready = g_workout_ready;
  snap.workout = g_wo_prof;
  return snap;
}

TimerMode timer_engine_mode(void) {
  return g_mode;
}

TimerMode timer_engine_simple_mode(void) {
  return g_simple_mode;
}

bool timer_engine_is_workout(void) {
  return g_mode == kModeWorkout;
}

uint32_t timer_engine_duration_sec(void) {
  return g_cd_duration_sec;
}

WorkoutProfile timer_engine_workout(void) {
  return g_wo_prof;
}

void timer_engine_set_workout(const WorkoutProfile& p) {
  g_wo_prof = timer_workout_clamp(p);
  reset_workout_idle();
}

void timer_engine_set_countdown_sec(uint32_t sec) {
  load_countdown_duration(sec);
}

void timer_engine_use_workout(bool on) {
  pause_if_running(active());
  if (on) {
    g_mode = kModeWorkout;
    reset_workout_idle();
  } else {
    g_mode = g_simple_mode;
  }
}

void timer_engine_set_workout_ready(bool ready) {
  g_workout_ready = ready;
}

bool timer_engine_workout_ready(void) {
  return g_workout_ready;
}

void timer_engine_pause(void) {
  pause_if_running(active());
}

void timer_engine_toggle_start_stop(void) {
  ModeState& s = active();

  if (g_mode == kModeWorkout && !g_workout_ready) {
    return;
  }

  if (g_mode == kModeCountdown && s.phase == kPhaseDone) {
    if (g_cd_duration_sec == 0) {
      return;
    }
    load_countdown_duration(g_cd_duration_sec);
    s.phase = kPhaseRunning;
    s.mark_ms = 0;
    return;
  }

  if (g_mode == kModeWorkout && s.phase == kPhaseDone) {
    reset_workout_idle();
    start_leg_running();
    return;
  }

  if (s.phase == kPhaseRunning) {
    s.phase = kPhasePaused;
    s.mark_ms = 0;
    return;
  }

  if (g_mode == kModeCountdown) {
    if (g_cd_duration_sec == 0) {
      return;
    }
    if (s.phase == kPhaseIdle && s.value_ms == 0) {
      load_countdown_duration(g_cd_duration_sec);
    }
  }

  if (g_mode == kModeWorkout && s.phase == kPhaseIdle && s.value_ms == 0) {
    reset_workout_idle();
  }

  s.phase = kPhaseRunning;
  s.mark_ms = 0;
}

void timer_engine_adjust_plus(void) {
  if (g_mode == kModeWorkout) return;
  ModeState& s = active();
  if (s.phase == kPhaseRunning) {
    return;
  }

  if (g_mode == kModeStopwatch) {
    s.value_ms = 0;
    s.phase = kPhaseIdle;
    s.mark_ms = 0;
    return;
  }

  uint32_t next = g_cd_duration_sec + kStepSec;
  if (next > kMaxSec) {
    next = kMaxSec;
  }
  load_countdown_duration(next);
}

void timer_engine_reset_zero(void) {
  if (g_mode == kModeWorkout) {
    timer_engine_reset_session();
    return;
  }
  ModeState& s = active();
  if (s.phase == kPhaseRunning) {
    return;
  }

  if (g_mode == kModeStopwatch) {
    s.value_ms = 0;
    s.phase = kPhaseIdle;
    s.mark_ms = 0;
    return;
  }

  load_countdown_duration(0);
}

void timer_engine_reset_session(void) {
  ModeState& s = active();
  if (s.phase == kPhaseRunning) {
    return;
  }
  if (g_mode == kModeWorkout) {
    reset_workout_idle();
    return;
  }
  timer_engine_reset_zero();
}

void timer_engine_switch_mode(void) {
  timer_engine_cycle_mode();
}

void timer_engine_cycle_mode(void) {
  pause_if_running(active());
  if (g_mode == kModeStopwatch) {
    g_simple_mode = kModeCountdown;
    g_mode = kModeCountdown;
  } else if (g_mode == kModeCountdown) {
    g_simple_mode = kModeCountdown;
    g_mode = kModeWorkout;
    reset_workout_idle();
  } else {
    g_simple_mode = kModeStopwatch;
    g_mode = kModeStopwatch;
  }
}
