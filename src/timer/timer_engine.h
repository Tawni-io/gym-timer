#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum TimerMode : uint8_t {
  kModeStopwatch = 0,
  kModeCountdown = 1,
  kModeWorkout = 2,
};

enum TimerPhase : uint8_t {
  kPhaseIdle = 0,
  kPhaseRunning = 1,
  kPhasePaused = 2,
  kPhaseDone = 3,  // countdown / workout
};

struct WorkoutProfile {
  uint8_t sets;
  uint16_t work_sec;
  uint16_t rest_sec;
};

struct TimerSnapshot {
  TimerMode mode;
  TimerPhase phase;
  uint32_t display_ms;      // elapsed (stopwatch) or remaining (countdown/interval)
  uint32_t duration_sec;    // loaded countdown length (multiple of 30, or 0)
  bool warning_last10;      // running, remaining <= 10 s
  uint8_t workout_set;      // 1-based
  uint8_t workout_sets;
  bool workout_resting;
  bool workout_ready;
  WorkoutProfile workout;
};

constexpr uint8_t kWorkoutSetsMin = 1;
constexpr uint8_t kWorkoutSetsMax = 20;
constexpr uint16_t kWorkoutWorkMinSec = 5;
constexpr uint16_t kWorkoutWorkMaxSec = 30 * 60;
constexpr uint16_t kWorkoutRestMaxSec = 10 * 60;

WorkoutProfile timer_workout_default(void);
WorkoutProfile timer_workout_clamp(WorkoutProfile p);
void timer_workout_format_line(char* buf, size_t n, const WorkoutProfile& p);

/** Countdown step / bounds (seconds). Min usable set length is 0 (cleared). */
uint32_t timer_adjust_step_sec(void);
uint32_t timer_countdown_max_sec(void);

void timer_engine_init(uint32_t countdown_duration_sec, const WorkoutProfile& workout,
                       bool workout_face, TimerMode simple_mode, bool workout_ready);

/** Advance running timers using wall clock (call every loop). */
void timer_engine_tick(uint32_t now_ms);

TimerSnapshot timer_engine_snapshot(void);
TimerMode timer_engine_mode(void);
TimerMode timer_engine_simple_mode(void);
bool timer_engine_is_workout(void);
uint32_t timer_engine_duration_sec(void);
WorkoutProfile timer_engine_workout(void);

void timer_engine_set_workout(const WorkoutProfile& p);
void timer_engine_set_countdown_sec(uint32_t sec);
void timer_engine_use_workout(bool on);
void timer_engine_set_workout_ready(bool ready);
bool timer_engine_workout_ready(void);
void timer_engine_pause(void);

void timer_engine_toggle_start_stop(void);
/** Stopwatch: reset to 0 when not running. Countdown: +30 s (clamp max), then load. */
void timer_engine_adjust_plus(void);
/** Stopwatch or countdown: clear to zero when not running. */
void timer_engine_reset_zero(void);
/** Workout: reload set 1 / work. Simple: same as reset_zero. No-op while running. */
void timer_engine_reset_session(void);
/** Pause active mode if running, then switch Timer ↔ Countdown (ignored in workout). */
void timer_engine_switch_mode(void);
/** Pause, then TIMER → COUNTDOWN → WORKOUT → TIMER. */
void timer_engine_cycle_mode(void);
