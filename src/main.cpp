#include <Arduino.h>
#include <esp_sleep.h>

#include "board_pins.h"
#include "config/timer_prefs.h"
#include "diag.h"
#include "display.h"
#include "softap/softap.h"
#include "timer/timer_engine.h"
#include "touch.h"
#include "ui/lvgl_port.h"
#include "ui/setup_ui.h"
#include "ui/splash.h"
#include "ui/timer_ui.h"

#ifndef TAWNI_GYM_VERSION
#define TAWNI_GYM_VERSION "0.1.0"
#endif

namespace {

constexpr uint32_t kSoftOffHoldMs = 3000;
constexpr uint32_t kWakeConfirmMs = 1500;
constexpr uint32_t kLongPressMs = 1500;
constexpr uint32_t kSetupHoldMs = 3000;
constexpr uint32_t kBothConfirmMs = 100;

enum Place : uint8_t {
  kPlaceFace = 0,
  kPlaceSetup = 1,
  kPlaceAp = 2,
};

Place g_place = kPlaceFace;
TimerPrefs g_prefs = {};

void persist_all(void) {
  g_prefs.countdown_sec = timer_engine_duration_sec();
  g_prefs.workout = timer_engine_workout();
  g_prefs.workout_ready = timer_engine_workout_ready();
  g_prefs.workout_face = timer_engine_is_workout();
  g_prefs.simple_mode = timer_engine_simple_mode();
  timer_prefs_save(g_prefs);
}

void persist_duration_if_changed(void) {
  const uint32_t sec = timer_engine_duration_sec();
  if (sec != g_prefs.countdown_sec) {
    timer_prefs_save_duration_sec(sec);
    g_prefs.countdown_sec = sec;
  }
}

void persist_face(void) {
  g_prefs.workout_face = timer_engine_is_workout();
  g_prefs.simple_mode = timer_engine_simple_mode();
  timer_prefs_save_face(g_prefs.workout_face, g_prefs.simple_mode);
}

void show_face(void) {
  timer_ui_show();
  g_place = kPlaceFace;
}

void apply_prefs_to_engine(void) {
  g_prefs = timer_prefs_load();
  timer_engine_set_countdown_sec(g_prefs.countdown_sec);
  timer_engine_set_workout(g_prefs.workout);
  timer_engine_set_workout_ready(g_prefs.workout_ready);
  timer_engine_use_workout(g_prefs.workout_face);
  if (g_prefs.flip) {
    display_reconfigure(g_prefs.flip);
  }
}

void enter_deep_sleep(void) {
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  Serial.println("Soft power → deep sleep (wake: hold bottom / GPIO0)");
  Serial.flush();
  delay(50);
  esp_deep_sleep_start();
}

void soft_power_off(void) {
  Serial.println("Soft power off");
  persist_all();
  if (softap_active()) {
    softap_stop();
  }
  display_enter_sleep();
  delay(100);
  enter_deep_sleep();
}

void soft_power_wake_confirm(void) {
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause != ESP_SLEEP_WAKEUP_GPIO) {
    return;
  }
  Serial.println("Wake from soft-off — hold bottom button (GPIO0) to stay on");
  const uint32_t start = millis();
  while ((millis() - start) < kWakeConfirmMs) {
    if (digitalRead(BUTTON_PIN) != LOW) {
      Serial.println("Wake confirm released → sleep again");
      enter_deep_sleep();
    }
    delay(10);
  }
  Serial.println("Wake confirm OK");
}

void enter_softap(void) {
  if (softap_active()) return;
  Serial.printf("Entering SoftAP (heap %u)\n", (unsigned)ESP.getFreeHeap());
  setup_ui_show_ap(kSoftApSsid, "192.168.4.1");
  for (int i = 0; i < 8; i++) {
    diag_wdt_feed();
    lvgl_port_handler();
    delay(5);
  }
  if (!softap_start()) {
    setup_ui_show_menu("WIFI FAILED");
    g_place = kPlaceSetup;
    return;
  }
  setup_ui_show_ap(kSoftApSsid, softap_ip());
  for (int i = 0; i < 10; i++) {
    diag_wdt_feed();
    lvgl_port_handler();
    delay(5);
  }
  lvgl_port_suspend_draw_buf();
  g_place = kPlaceAp;
}

void leave_softap(void) {
  if (!softap_active()) {
    show_face();
    return;
  }
  Serial.println("SoftAP leave");
  softap_stop();
  softap_clear_stop_request();
  const bool changed = softap_config_changed();
  softap_clear_config_changed();
  if (changed) {
    apply_prefs_to_engine();
  }

  if (!lvgl_port_resume_draw_buf()) {
    Serial.println("LVGL resume failed — restarting to reclaim heap");
    delay(200);
    ESP.restart();
  }

  const uint32_t settle_until = millis() + 400;
  while (millis() < settle_until) {
    diag_wdt_feed();
    lvgl_port_handler();
    delay(10);
  }

  show_face();
  Serial.println("SoftAP leave → timer UI");
}

void on_mode_switch(void) {
  if (g_place != kPlaceFace) return;
  timer_engine_cycle_mode();
  persist_face();
  const TimerMode m = timer_engine_mode();
  Serial.printf("mode → %s\n", m == kModeStopwatch ? "TIMER" : (m == kModeCountdown ? "COUNTDOWN" : "WORKOUT"));
}

void on_reset(void) {
  if (g_place != kPlaceFace) return;
  timer_engine_reset_zero();
  Serial.println("reset");
}

void on_plus30(void) {
  if (g_place != kPlaceFace) return;
  if (timer_engine_mode() != kModeCountdown) return;
  timer_engine_adjust_plus();
  persist_duration_if_changed();
  Serial.printf("+30s → %lus\n", (unsigned long)timer_engine_duration_sec());
}

void toggle_setup(void) {
  if (g_place == kPlaceAp || softap_active()) {
    Serial.println("GPIO0 long → leave setup");
    leave_softap();
    return;
  }
  if (g_place == kPlaceSetup) {
    Serial.println("GPIO0 long → leave setup");
    show_face();
    return;
  }
  timer_engine_pause();
  Serial.println("GPIO0 long → setup");
  enter_softap();
}

void poll_buttons(void) {
  static bool was_top = false, was_bot = false;
  static uint32_t down_top_ms = 0, down_bot_ms = 0, both_ms = 0;
  static bool long_top_fired = false;
  static bool long_bot_fired = false;
  static bool reset_fired = false;
  static bool soft_off_fired = false;

  const uint32_t now = millis();
  const bool down_top = digitalRead(BUTTON_BOOT) == LOW;
  const bool down_bot = digitalRead(BUTTON_PIN) == LOW;
  const bool in_setup = (g_place == kPlaceAp || g_place == kPlaceSetup || softap_active());

  if (down_top && down_bot) {
    if (both_ms == 0) both_ms = now;
    if ((now - both_ms) >= kBothConfirmMs) {
      long_top_fired = true;
      long_bot_fired = true;
    }
    if (!soft_off_fired && (now - both_ms) >= kSoftOffHoldMs) {
      soft_off_fired = true;
      soft_power_off();
    }
  } else {
    both_ms = 0;
    soft_off_fired = false;
  }

  // Top alone: short = countdown +30; long = mode.
  if (down_top && !down_bot) {
    if (!was_top) {
      down_top_ms = now;
      long_top_fired = false;
    }
    if (!in_setup && !long_top_fired && (now - down_top_ms) >= kLongPressMs) {
      long_top_fired = true;
      on_mode_switch();
    }
  }

  // Bottom alone. Cabin: short = start/pause, ~1.5 s = reset, keep holding ~3 s = unlabeled setup.
  // Setup overlay: ~1.5 s hold leaves.
  if (down_bot && !down_top) {
    if (!was_bot) {
      down_bot_ms = now;
      long_bot_fired = false;
      reset_fired = false;
    }
    if (!long_bot_fired) {
      const uint32_t held = now - down_bot_ms;
      if (in_setup && held >= kLongPressMs) {
        long_bot_fired = true;
        toggle_setup();
      } else if (!in_setup && held >= kSetupHoldMs) {
        long_bot_fired = true;
        if (reset_fired) {
          timer_engine_set_countdown_sec(g_prefs.countdown_sec);
          if (timer_engine_is_workout()) {
            timer_engine_reset_session();
          }
        }
        toggle_setup();
      } else if (!in_setup && !reset_fired && held >= kLongPressMs) {
        reset_fired = true;
        on_reset();
      }
    }
  }

  if (!down_top && was_top && !down_bot) {
    if (!long_top_fired) {
      on_plus30();
    }
  }

  if (!down_bot && was_bot && !down_top) {
    if (!long_bot_fired && !in_setup) {
      if (reset_fired) {
        persist_duration_if_changed();
      } else {
        timer_engine_toggle_start_stop();
        const TimerSnapshot s = timer_engine_snapshot();
        Serial.printf("start/stop → phase=%u ms=%lu\n", (unsigned)s.phase,
                      (unsigned long)s.display_ms);
      }
    }
  }

  was_top = down_top;
  was_bot = down_bot;
}

void poll_touch(void) {
#if USE_TOUCH_SWIPE
  if (g_place != kPlaceFace) return;
  const TouchNav nav = touch_poll();
  if (nav == kTouchNavNext || nav == kTouchNavPrev) {
    on_mode_switch();
  }
#endif
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(200);
  Serial.printf("\nGym Timer %s\n", TAWNI_GYM_VERSION);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON_BOOT, INPUT_PULLUP);

  soft_power_wake_confirm();
  diag_begin();

  g_prefs = timer_prefs_load();

  if (!display_init()) {
    Serial.println("FATAL: display_init failed");
    while (true) delay(1000);
  }
  if (g_prefs.flip) {
    display_reconfigure(g_prefs.flip);
  }

  if (!lvgl_port_init()) {
    Serial.println("FATAL: lvgl_port_init failed");
    while (true) delay(1000);
  }

  splash_show();
  Serial.printf("Splash (wake cause %d)\n", (int)esp_sleep_get_wakeup_cause());
  {
    const uint32_t start = millis();
    while ((millis() - start) < kWakeConfirmMs) {
      lvgl_port_handler();
      delay(10);
    }
  }

  touch_init();
  board_power_init();

  timer_engine_init(g_prefs.countdown_sec, g_prefs.workout, g_prefs.workout_face,
                    g_prefs.simple_mode, g_prefs.workout_ready);
  timer_ui_init();
  setup_ui_init();

  Serial.println("Gym timer ready");
  Serial.println("TOP  short=+30s (countdown)  long=mode");
  Serial.println("BOT  short=start/stop  long=reset  hold ~3s=setup");
  Serial.println("Both 3s=soft-off  Wake: hold bottom 1.5s");
}

void loop() {
  const uint32_t now = millis();
  diag_wdt_feed();
  poll_buttons();
  poll_touch();

  if (g_place == kPlaceAp || softap_active()) {
    softap_loop();
    if (softap_stop_requested()) {
      Serial.println("SoftAP /stop requested");
      leave_softap();
    }
    delay(5);
    return;
  }

  timer_engine_tick(now);
  if (g_place == kPlaceFace) {
    timer_ui_update(timer_engine_snapshot(), now);
  }
  lvgl_port_handler();
  delay(5);
}
