#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "timer/timer_engine.h"

struct TimerPrefs {
  uint32_t countdown_sec;
  WorkoutProfile workout;
  bool workout_ready;  // true after a workout is saved from SoftAP
  bool workout_face;
  TimerMode simple_mode;
  uint8_t flip;  // 0 = normal, 1 = 180
};

TimerPrefs timer_prefs_load(void);
void timer_prefs_save(const TimerPrefs& p);

/** Load last countdown duration in seconds (30 s steps). Defaults to 30. */
uint32_t timer_prefs_load_duration_sec(void);

/** Persist countdown duration (seconds). */
void timer_prefs_save_duration_sec(uint32_t sec);

void timer_prefs_save_workout(const WorkoutProfile& p);
void timer_prefs_save_workout_ready(bool ready);
void timer_prefs_save_face(bool workout_face, TimerMode simple_mode);
void timer_prefs_save_flip(uint8_t flip);
