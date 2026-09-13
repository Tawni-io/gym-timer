#include "ui/timer_ui.h"

#ifndef TIMER_FACE_CLASSIC
#define TIMER_FACE_CLASSIC 0
#endif

#if TIMER_FACE_CLASSIC
#include "ui/timer_ui_classic.inc"
#else

#include <stdio.h>
#include <lvgl.h>

#include "board_pins.h"
#include "touch.h"

namespace {

// VictronBLE cabin palette (src/ui/dashboard.cpp).
constexpr uint32_t kBg = 0x0D1117;
constexpr uint32_t kCard = 0x161B22;
constexpr uint32_t kDim = 0x30363D;
constexpr uint32_t kFg = 0xFFFFFF;
constexpr uint32_t kMuted = 0x8B949E;
constexpr uint32_t kOk = 0x00E676;     // not lime — reads yellow on ST7789
constexpr uint32_t kWarn = 0xFFD600;
constexpr uint32_t kAlarm = 0xFF1744;
constexpr uint32_t kCyan = 0x00BCD4;   // TIMER identity
constexpr uint32_t kMint = 0x1DE9B6;   // COUNTDOWN identity
constexpr uint32_t kOrange = 0xFF9100;

constexpr int kRailW = 58;
constexpr int kStripH = 3;
constexpr int kLetterSpace = 0;
constexpr uint32_t kBattPollMs = 5000;

lv_obj_t* g_scr = nullptr;
lv_obj_t* g_strip = nullptr;
lv_obj_t* g_family = nullptr;
lv_obj_t* g_pill = nullptr;
lv_obj_t* g_pill_dot = nullptr;
lv_obj_t* g_pill_lbl = nullptr;
lv_obj_t* g_card_top = nullptr;
lv_obj_t* g_card_bot = nullptr;
lv_obj_t* g_card_top_acc = nullptr;
lv_obj_t* g_card_bot_acc = nullptr;
lv_obj_t* g_hint_top_short = nullptr;
lv_obj_t* g_hint_top_long = nullptr;
lv_obj_t* g_hint_bot_short = nullptr;
lv_obj_t* g_hint_bot_long = nullptr;
lv_obj_t* g_time_row = nullptr;
lv_obj_t* g_help = nullptr;
lv_obj_t* g_help_l1 = nullptr;
lv_obj_t* g_help_l2 = nullptr;
lv_obj_t* g_help_l3 = nullptr;
lv_obj_t* g_batt_lbl = nullptr;
lv_obj_t* g_d[6] = {};
lv_obj_t* g_colon[2] = {};
bool g_ready = false;

uint32_t g_last_flash_ms = 0;
bool g_flash_on = false;
uint32_t g_last_drawn_cs = UINT32_MAX;
uint8_t g_last_phase = 0xFF;
uint8_t g_last_mode = 0xFF;
uint8_t g_last_wo_set = 0xFF;
bool g_last_wo_rest = false;
bool g_last_wo_ready = true;
bool g_last_flash_lit = true;
uint32_t g_last_batt_ms = 0;
int g_last_batt_pct = -999;
bool g_last_batt_chg = false;

lv_coord_t g_digit_w = 30;
lv_coord_t g_colon_w = 16;

void strip_chrome(lv_obj_t* o) {
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
}

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
  lv_obj_set_style_pad_all(lbl, 0, 0);
  return lbl;
}

lv_obj_t* make_fixed_digit(lv_obj_t* parent, lv_coord_t w, uint32_t color, const char* text) {
  lv_obj_t* lbl = make_label(parent, &lv_font_montserrat_48, color);
  lv_obj_set_width(lbl, w);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(lbl, kLetterSpace, 0);
  lv_label_set_text(lbl, text);
  return lbl;
}

lv_obj_t* make_card(lv_obj_t* parent, int x, int y, int w, int h, uint32_t accent,
                    lv_obj_t** accent_out) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  strip_chrome(card);
  lv_obj_set_style_bg_color(card, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

  lv_obj_t* bar = lv_obj_create(card);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_size(bar, w, kStripH);
  strip_chrome(bar);
  lv_obj_set_style_bg_color(bar, lv_color_hex(accent), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  if (accent_out) *accent_out = bar;
  return card;
}

void style_hint_short(lv_obj_t* lbl) {
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(kFg), 0);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lbl, LV_SIZE_CONTENT);
}

void style_hint_long(lv_obj_t* lbl) {
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(kMuted), 0);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lbl, LV_SIZE_CONTENT);
}

const char* phase_label(TimerPhase p) {
  switch (p) {
    case kPhaseRunning:
      return "RUN";
    case kPhasePaused:
      return "PAUSE";
    case kPhaseDone:
      return "DONE";
    default:
      return "READY";
  }
}

uint32_t mode_accent(const TimerSnapshot& snap) {
  if (snap.mode == kModeWorkout) {
    return snap.workout_resting ? kMint : kOrange;
  }
  return snap.mode == kModeCountdown ? kMint : kCyan;
}

uint32_t phase_color(TimerPhase p) {
  if (p == kPhaseRunning) return kOk;
  if (p == kPhasePaused) return kWarn;
  if (p == kPhaseDone) return kAlarm;
  return kMuted;
}

void update_hints(const TimerSnapshot& snap) {
  if (snap.mode == kModeCountdown && snap.phase != kPhaseRunning) {
    lv_label_set_text(g_hint_top_short, "+30");
    lv_label_set_text(g_hint_top_long, "MODE");
    lv_obj_align(g_hint_top_short, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_clear_flag(g_hint_top_long, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(g_hint_top_short, "MODE");
    lv_label_set_text(g_hint_top_long, "");
    lv_obj_align(g_hint_top_short, LV_ALIGN_CENTER, 0, 2);
    lv_obj_add_flag(g_hint_top_long, LV_OBJ_FLAG_HIDDEN);
  }

  if (snap.mode == kModeWorkout && !snap.workout_ready) {
    lv_label_set_text(g_hint_bot_short, "START");
    lv_label_set_text(g_hint_bot_long, "RESET");
    return;
  }

  if (snap.phase == kPhaseRunning) {
    lv_label_set_text(g_hint_bot_short, "STOP");
  } else {
    lv_label_set_text(g_hint_bot_short, "START");
  }
  lv_label_set_text(g_hint_bot_long, "RESET");
}

void show_workout_help(bool on) {
  if (!g_help || !g_time_row) return;
  if (on) {
    lv_obj_add_flag(g_time_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_help, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(g_time_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_help, LV_OBJ_FLAG_HIDDEN);
  }
}

void set_time_colors(lv_color_t c_min, lv_color_t c_sec, lv_color_t c_cs) {
  lv_obj_set_style_text_color(g_d[0], c_min, 0);
  lv_obj_set_style_text_color(g_d[1], c_min, 0);
  lv_obj_set_style_text_color(g_colon[0], c_sec, 0);
  lv_obj_set_style_text_color(g_d[2], c_sec, 0);
  lv_obj_set_style_text_color(g_d[3], c_sec, 0);
  lv_obj_set_style_text_color(g_colon[1], c_cs, 0);
  lv_obj_set_style_text_color(g_d[4], c_cs, 0);
  lv_obj_set_style_text_color(g_d[5], c_cs, 0);
}

void set_digit(lv_obj_t* lbl, int v) {
  char b[2] = {(char)('0' + (v % 10)), 0};
  lv_label_set_text(lbl, b);
}

int soc_from_mv(int mV) {
  if (mV <= 3300) return 0;
  if (mV >= 4200) return 100;
  return ((mV - 3300) * 100) / 900;
}

uint32_t batt_color(int pct, bool charging) {
  if (charging) return kOk;
  if (pct <= 10) return kAlarm;
  if (pct <= 20) return kWarn;
  return kOk;
}

void apply_batt_ui(int pct, bool charging, bool known) {
  const uint32_t col = known ? batt_color(pct, charging) : kMuted;
  lv_obj_set_style_text_color(g_batt_lbl, lv_color_hex(col), 0);

  if (!known) {
    lv_label_set_text(g_batt_lbl, "--%");
    return;
  }

  char buf[8];
  if (charging) {
    snprintf(buf, sizeof(buf), "%d%%+", pct);
  } else {
    snprintf(buf, sizeof(buf), "%d%%", pct);
  }
  lv_label_set_text(g_batt_lbl, buf);
}

void update_battery(uint32_t now_ms, bool force) {
  if (!force && (now_ms - g_last_batt_ms) < kBattPollMs) return;
  g_last_batt_ms = now_ms;

  int pct = -1;
  bool charging = false;
  bool known = false;

  if (board_power_present()) {
    int mV = 0;
    int soc = -1;
    int i_mA = 0;
    if (board_power_read(&mV, &soc, &i_mA)) {
      charging = i_mA > 0;
      if (soc >= 0 && soc <= 100) {
        pct = soc;
      } else {
        pct = soc_from_mv(mV);
      }
      known = true;
    }
  }

  if (!force && known && pct == g_last_batt_pct && charging == g_last_batt_chg) return;
  if (!force && !known && g_last_batt_pct == -1 && !g_last_batt_chg) return;

  g_last_batt_pct = known ? pct : -1;
  g_last_batt_chg = charging;
  apply_batt_ui(pct, charging, known);
}

}  // namespace

bool timer_ui_init(void) {
  lv_obj_t* prev = lv_screen_active();
  g_scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(g_scr, lv_color_hex(kBg), 0);
  lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_scr, 0, 0);
  lv_obj_set_style_pad_all(g_scr, 0, 0);
  lv_obj_set_style_radius(g_scr, 0, 0);
  lv_obj_clear_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);

  g_digit_w = 0;
  for (char d = '0'; d <= '9'; d++) {
    char s[2] = {d, 0};
    const lv_coord_t w = lv_text_get_width(s, 1, &lv_font_montserrat_48, kLetterSpace);
    if (w > g_digit_w) g_digit_w = w;
  }
  g_digit_w += 2;
  g_colon_w = lv_text_get_width(":", 1, &lv_font_montserrat_48, kLetterSpace) + 4;

  g_strip = lv_obj_create(g_scr);
  lv_obj_set_pos(g_strip, 0, 0);
  lv_obj_set_size(g_strip, DISPLAY_WIDTH, kStripH);
  strip_chrome(g_strip);
  lv_obj_set_style_bg_color(g_strip, lv_color_hex(kCyan), 0);
  lv_obj_set_style_bg_opa(g_strip, LV_OPA_COVER, 0);

  g_family = make_label(g_scr, &lv_font_montserrat_12, kCyan);
  lv_obj_set_style_text_letter_space(g_family, 1, 0);
  lv_label_set_text(g_family, "TIMER");
  lv_obj_align(g_family, LV_ALIGN_TOP_LEFT, 10, 8);

  g_pill = lv_obj_create(g_scr);
  lv_obj_set_size(g_pill, 72, 20);
  lv_obj_align(g_pill, LV_ALIGN_TOP_RIGHT, -(kRailW + 8), 6);
  strip_chrome(g_pill);
  lv_obj_set_style_bg_color(g_pill, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(g_pill, LV_OPA_COVER, 0);

  g_pill_dot = lv_obj_create(g_pill);
  lv_obj_set_size(g_pill_dot, 6, 6);
  lv_obj_set_pos(g_pill_dot, 6, 7);
  strip_chrome(g_pill_dot);
  lv_obj_set_style_bg_color(g_pill_dot, lv_color_hex(kMuted), 0);
  lv_obj_set_style_bg_opa(g_pill_dot, LV_OPA_COVER, 0);

  g_pill_lbl = make_label(g_pill, &lv_font_montserrat_12, kFg);
  lv_obj_set_pos(g_pill_lbl, 16, 2);
  lv_label_set_text(g_pill_lbl, "READY");

  const int rail_x = DISPLAY_WIDTH - kRailW;
  const int card_w = kRailW - 6;
  const int card_h = 72;
  g_card_top = make_card(g_scr, rail_x, 8, card_w, card_h, kCyan, &g_card_top_acc);
  g_card_bot = make_card(g_scr, rail_x, DISPLAY_HEIGHT - 8 - card_h, card_w, card_h, kOrange,
                        &g_card_bot_acc);

  g_hint_top_short = lv_label_create(g_card_top);
  style_hint_short(g_hint_top_short);
  lv_label_set_text(g_hint_top_short, "+30");
  lv_obj_align(g_hint_top_short, LV_ALIGN_TOP_MID, 0, 18);

  g_hint_top_long = lv_label_create(g_card_top);
  style_hint_long(g_hint_top_long);
  lv_label_set_text(g_hint_top_long, "MODE");
  lv_obj_align(g_hint_top_long, LV_ALIGN_BOTTOM_MID, 0, -12);

  g_hint_bot_short = lv_label_create(g_card_bot);
  style_hint_short(g_hint_bot_short);
  lv_label_set_text(g_hint_bot_short, "START");
  lv_obj_align(g_hint_bot_short, LV_ALIGN_TOP_MID, 0, 18);

  g_hint_bot_long = lv_label_create(g_card_bot);
  style_hint_long(g_hint_bot_long);
  lv_label_set_text(g_hint_bot_long, "RESET");
  lv_obj_align(g_hint_bot_long, LV_ALIGN_BOTTOM_MID, 0, -12);

  g_time_row = lv_obj_create(g_scr);
  lv_obj_set_size(g_time_row, DISPLAY_WIDTH - kRailW, 64);
  lv_obj_align(g_time_row, LV_ALIGN_LEFT_MID, 0, 2);
  lv_obj_set_style_bg_opa(g_time_row, LV_OPA_TRANSP, 0);
  strip_chrome(g_time_row);
  lv_obj_set_flex_flow(g_time_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(g_time_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(g_time_row, 0, 0);

  g_d[0] = make_fixed_digit(g_time_row, g_digit_w, kFg, "0");
  g_d[1] = make_fixed_digit(g_time_row, g_digit_w, kFg, "0");
  g_colon[0] = make_fixed_digit(g_time_row, g_colon_w, kDim, ":");
  g_d[2] = make_fixed_digit(g_time_row, g_digit_w, kFg, "0");
  g_d[3] = make_fixed_digit(g_time_row, g_digit_w, kFg, "0");
  g_colon[1] = make_fixed_digit(g_time_row, g_colon_w, kMuted, ":");
  g_d[4] = make_fixed_digit(g_time_row, g_digit_w, kMuted, "0");
  g_d[5] = make_fixed_digit(g_time_row, g_digit_w, kMuted, "0");

  g_help = lv_obj_create(g_scr);
  lv_obj_set_size(g_help, DISPLAY_WIDTH - kRailW - 16, 88);
  lv_obj_align(g_help, LV_ALIGN_LEFT_MID, 8, 4);
  strip_chrome(g_help);
  lv_obj_set_style_bg_color(g_help, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(g_help, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(g_help, 8, 0);
  lv_obj_add_flag(g_help, LV_OBJ_FLAG_HIDDEN);

  g_help_l1 = make_label(g_help, &lv_font_montserrat_14, kFg);
  lv_label_set_text(g_help_l1, "No workout set");
  lv_obj_align(g_help_l1, LV_ALIGN_TOP_LEFT, 0, 0);

  g_help_l2 = make_label(g_help, &lv_font_montserrat_12, kMuted);
  lv_label_set_text(g_help_l2, "Hold bottom ~3s for setup");
  lv_obj_align(g_help_l2, LV_ALIGN_TOP_LEFT, 0, 24);

  g_help_l3 = make_label(g_help, &lv_font_montserrat_12, kCyan);
  lv_label_set_text(g_help_l3, "Join GymTimer  192.168.4.1");
  lv_obj_align(g_help_l3, LV_ALIGN_TOP_LEFT, 0, 44);

  g_batt_lbl = make_label(g_scr, &lv_font_montserrat_12, kMuted);
  lv_obj_align(g_batt_lbl, LV_ALIGN_BOTTOM_LEFT, 10, -6);
  lv_label_set_text(g_batt_lbl, "--%");

  lv_screen_load(g_scr);
  lv_obj_invalidate(g_scr);
  if (prev && prev != g_scr) {
    lv_obj_delete(prev);
  }

  g_ready = true;
  update_battery(0, true);
  return true;
}

void timer_ui_show(void) {
  if (!g_ready || !g_scr) return;
  lv_screen_load(g_scr);
  lv_obj_invalidate(g_scr);
}

void timer_ui_update(const TimerSnapshot& snap, uint32_t now_ms) {
  if (!g_ready) return;

  if (snap.phase == kPhaseDone) {
    if (now_ms - g_last_flash_ms >= 280) {
      g_last_flash_ms = now_ms;
      g_flash_on = !g_flash_on;
    }
  } else {
    g_flash_on = true;
  }

  const uint32_t acc = mode_accent(snap);
  const uint32_t st = phase_color(snap.phase);

  if (snap.mode == kModeWorkout) {
    lv_label_set_text(g_family, "WORKOUT");
    if (!snap.workout_ready) {
      lv_label_set_text(g_pill_lbl, "---");
      show_workout_help(true);
    } else {
      if (snap.workout_resting) {
        lv_label_set_text(g_family, "REST");
      } else if (snap.phase != kPhaseIdle) {
        lv_label_set_text(g_family, "WORK");
      }
      char pill[8];
      snprintf(pill, sizeof(pill), "%u/%u", (unsigned)snap.workout_set,
               (unsigned)snap.workout_sets);
      lv_label_set_text(g_pill_lbl, pill);
      show_workout_help(false);
    }
  } else {
    show_workout_help(false);
    lv_label_set_text(g_family, snap.mode == kModeStopwatch ? "TIMER" : "COUNTDOWN");
    lv_label_set_text(g_pill_lbl, phase_label(snap.phase));
  }
  lv_obj_set_style_text_color(g_family, lv_color_hex(acc), 0);
  lv_obj_set_style_bg_color(g_strip, lv_color_hex(acc), 0);
  lv_obj_set_style_bg_color(g_card_top_acc, lv_color_hex(acc), 0);

  lv_obj_set_style_bg_color(g_pill_dot, lv_color_hex(st), 0);
  lv_obj_set_style_text_color(g_pill_lbl, lv_color_hex(st == kMuted ? kFg : st), 0);

  uint32_t bot_acc = kOrange;
  if (snap.phase == kPhaseRunning) bot_acc = kOk;
  else if (snap.phase == kPhasePaused) bot_acc = kWarn;
  else if (snap.phase == kPhaseDone) bot_acc = kAlarm;
  lv_obj_set_style_bg_color(g_card_bot_acc, lv_color_hex(bot_acc), 0);

  update_hints(snap);
  update_battery(now_ms, false);

  const uint32_t cs_total = snap.display_ms / 10u;
  if (cs_total == g_last_drawn_cs && snap.phase == g_last_phase && g_flash_on == g_last_flash_lit &&
      (uint8_t)snap.mode == g_last_mode && snap.workout_set == g_last_wo_set &&
      snap.workout_resting == g_last_wo_rest && snap.workout_ready == g_last_wo_ready) {
    return;
  }
  g_last_drawn_cs = cs_total;
  g_last_phase = (uint8_t)snap.phase;
  g_last_mode = (uint8_t)snap.mode;
  g_last_wo_set = snap.workout_set;
  g_last_wo_rest = snap.workout_resting;
  g_last_wo_ready = snap.workout_ready;
  g_last_flash_lit = g_flash_on;

  if (snap.mode == kModeWorkout && !snap.workout_ready) {
    return;
  }

  const uint32_t cs = cs_total % 100u;
  const uint32_t total_sec = snap.display_ms / 1000u;
  const uint32_t mins = (total_sec / 60u) % 100u;
  const uint32_t secs = total_sec % 60u;

  set_digit(g_d[0], (int)((mins / 10) % 10));
  set_digit(g_d[1], (int)(mins % 10));
  set_digit(g_d[2], (int)(secs / 10));
  set_digit(g_d[3], (int)(secs % 10));
  set_digit(g_d[4], (int)(cs / 10));
  set_digit(g_d[5], (int)(cs % 10));

  if (snap.phase == kPhaseDone && !g_flash_on) {
    set_time_colors(lv_color_hex(0x4A1018), lv_color_hex(0x4A1018), lv_color_hex(0x3A0C12));
  } else if (snap.warning_last10) {
    const lv_color_t c = lv_color_hex(kAlarm);
    set_time_colors(c, c, c);
  } else if (snap.phase == kPhasePaused) {
    const lv_color_t c = lv_color_hex(kWarn);
    set_time_colors(c, c, lv_color_hex(0xB39600));
  } else if (snap.phase == kPhaseRunning) {
    set_time_colors(lv_color_hex(kFg), lv_color_hex(kFg), lv_color_hex(acc));
  } else {
    set_time_colors(lv_color_hex(kFg), lv_color_hex(kFg), lv_color_hex(kMuted));
  }
}

#endif  // !TIMER_FACE_CLASSIC
