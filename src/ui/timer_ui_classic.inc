// Classic timer face (pre-0.4.0). Included from timer_ui.cpp when TIMER_FACE_CLASSIC=1.
#include "ui/timer_ui.h"

#include <stdio.h>
#include <lvgl.h>

#include "board_pins.h"
#include "touch.h"

namespace {

// Cabin chrome + classic timer accents (green / red / white / amber).
constexpr uint32_t kColBg = 0x0D1117;
constexpr uint32_t kColGreen = 0x00E676;  // minutes, RUN, charging (not lime — reads yellow on ST7789)
constexpr uint32_t kColRed = 0xFF2A2A;    // seconds, CS, DONE / critical batt
constexpr uint32_t kColAmber = 0xFFC400;  // pause
constexpr uint32_t kColWhite = 0xFFFFFF;
constexpr uint32_t kColMuted = 0x8B949E;
constexpr uint32_t kColSoft = 0xC9D1D9;
constexpr uint32_t kColRail = 0x161B22;
constexpr uint32_t kColLine = 0x30363D;

constexpr int kRailW = 52;
constexpr int kRailAccentW = 2;
constexpr int kLetterSpace = 0;
constexpr uint32_t kBattPollMs = 5000;
constexpr int kBattBodyW = 18;
constexpr int kBattBodyH = 10;
constexpr int kBattTipW = 2;
constexpr int kBattTipH = 5;
constexpr int kBattPad = 2;
constexpr int kBattInnerW = kBattBodyW - (kBattPad * 2);

lv_obj_t* g_scr = nullptr;
lv_obj_t* g_rail = nullptr;
lv_obj_t* g_rail_accent = nullptr;
lv_obj_t* g_rail_mid = nullptr;
lv_obj_t* g_mode_lbl = nullptr;
lv_obj_t* g_status_lbl = nullptr;
lv_obj_t* g_hint_top = nullptr;
lv_obj_t* g_hint_bot = nullptr;
lv_obj_t* g_time_row = nullptr;
lv_obj_t* g_help_lbl = nullptr;
lv_obj_t* g_batt_row = nullptr;
lv_obj_t* g_batt_body = nullptr;
lv_obj_t* g_batt_fill = nullptr;
lv_obj_t* g_batt_tip = nullptr;
lv_obj_t* g_batt_lbl = nullptr;
// Fixed slots: [M0 M1] : [S0 S1] : [C0 C1] — each digit has equal width so the line never shifts.
lv_obj_t* g_d[6] = {};
lv_obj_t* g_colon[2] = {};
bool g_ready = false;

uint32_t g_last_flash_ms = 0;
bool g_flash_on = false;
uint32_t g_last_drawn_cs = UINT32_MAX;
uint8_t g_last_phase = 0xFF;
uint8_t g_last_mode = 0xFF;
bool g_last_wo_ready = true;
bool g_last_flash_lit = true;
uint32_t g_last_batt_ms = 0;
int g_last_batt_pct = -999;
bool g_last_batt_chg = false;

lv_coord_t g_digit_w = 30;
lv_coord_t g_colon_w = 16;

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

lv_obj_t* make_fixed_label(lv_obj_t* parent, lv_coord_t w, uint32_t color, const char* text) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_width(lbl, w);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(lbl, kLetterSpace, 0);
  lv_obj_set_style_pad_all(lbl, 0, 0);
  lv_label_set_text(lbl, text);
  return lbl;
}

void style_hint(lv_obj_t* lbl) {
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(kColMuted), 0);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(lbl, 0, 0);
  // Content-sized: fixed narrow width was wrapping the last glyph of START/RESET.
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lbl, LV_SIZE_CONTENT);
}

void update_hints(const TimerSnapshot& snap) {
  if (snap.mode == kModeCountdown) {
    if (snap.phase == kPhaseRunning) {
      lv_label_set_text(g_hint_top, "MODE");
      lv_label_set_text(g_hint_bot, "STOP\n\nRESET");
    } else {
      lv_label_set_text(g_hint_top, "+30\n\nMODE");
      lv_label_set_text(g_hint_bot, "START\n\nRESET");
    }
  } else {
    lv_label_set_text(g_hint_top, "MODE");
    if (snap.phase == kPhaseRunning) {
      lv_label_set_text(g_hint_bot, "STOP\n\nRESET");
    } else {
      lv_label_set_text(g_hint_bot, "START\n\nRESET");
    }
  }
}

void set_time_colors(lv_color_t c_min, lv_color_t c_rest) {
  lv_obj_set_style_text_color(g_d[0], c_min, 0);
  lv_obj_set_style_text_color(g_d[1], c_min, 0);
  lv_obj_set_style_text_color(g_colon[0], c_rest, 0);
  lv_obj_set_style_text_color(g_d[2], c_rest, 0);
  lv_obj_set_style_text_color(g_d[3], c_rest, 0);
  lv_obj_set_style_text_color(g_colon[1], c_rest, 0);
  lv_obj_set_style_text_color(g_d[4], c_rest, 0);
  lv_obj_set_style_text_color(g_d[5], c_rest, 0);
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
  if (charging) return kColGreen;
  if (pct <= 10) return kColRed;
  if (pct <= 20) return kColAmber;
  return kColSoft;
}

void apply_batt_ui(int pct, bool charging, bool known) {
  const uint32_t col = known ? batt_color(pct, charging) : kColMuted;
  lv_obj_set_style_border_color(g_batt_body, lv_color_hex(col), 0);
  lv_obj_set_style_bg_color(g_batt_tip, lv_color_hex(col), 0);
  lv_obj_set_style_text_color(g_batt_lbl, lv_color_hex(col), 0);

  if (!known) {
    lv_obj_set_width(g_batt_fill, 0);
    lv_label_set_text(g_batt_lbl, "--%");
    return;
  }

  int fill_w = (kBattInnerW * pct) / 100;
  if (pct > 0 && fill_w < 1) fill_w = 1;
  lv_obj_set_width(g_batt_fill, fill_w);
  lv_obj_set_style_bg_color(g_batt_fill, lv_color_hex(col), 0);

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
  lv_obj_set_style_bg_color(g_scr, lv_color_hex(kColBg), 0);
  lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_scr, 0, 0);
  lv_obj_set_style_pad_all(g_scr, 0, 0);
  lv_obj_set_style_radius(g_scr, 0, 0);
  lv_obj_clear_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);

  // Lock every digit cell to the widest glyph (usually "0"/"8"/"4") so "1" never shifts the row.
  g_digit_w = 0;
  for (char d = '0'; d <= '9'; d++) {
    char s[2] = {d, 0};
    const lv_coord_t w = lv_text_get_width(s, 1, &lv_font_montserrat_48, kLetterSpace);
    if (w > g_digit_w) g_digit_w = w;
  }
  g_digit_w += 2;
  g_colon_w = lv_text_get_width(":", 1, &lv_font_montserrat_48, kLetterSpace) + 4;

  // Right chrome: Victron-style card surface + hairline + mid rule (top/bottom buttons).
  g_rail = lv_obj_create(g_scr);
  lv_obj_set_size(g_rail, kRailW, DISPLAY_HEIGHT);
  lv_obj_align(g_rail, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(g_rail, lv_color_hex(kColRail), 0);
  lv_obj_set_style_bg_opa(g_rail, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(g_rail, LV_BORDER_SIDE_LEFT, 0);
  lv_obj_set_style_border_width(g_rail, 1, 0);
  lv_obj_set_style_border_color(g_rail, lv_color_hex(kColLine), 0);
  lv_obj_set_style_radius(g_rail, 0, 0);
  lv_obj_set_style_pad_all(g_rail, 0, 0);
  lv_obj_clear_flag(g_rail, LV_OBJ_FLAG_SCROLLABLE);

  g_rail_accent = lv_obj_create(g_rail);
  lv_obj_set_size(g_rail_accent, kRailAccentW, DISPLAY_HEIGHT);
  lv_obj_align(g_rail_accent, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_bg_color(g_rail_accent, lv_color_hex(kColLine), 0);
  lv_obj_set_style_bg_opa(g_rail_accent, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_rail_accent, 0, 0);
  lv_obj_set_style_radius(g_rail_accent, 0, 0);
  lv_obj_set_style_pad_all(g_rail_accent, 0, 0);
  lv_obj_clear_flag(g_rail_accent, LV_OBJ_FLAG_SCROLLABLE);

  g_rail_mid = lv_obj_create(g_rail);
  lv_obj_set_size(g_rail_mid, kRailW - 16, 1);
  lv_obj_align(g_rail_mid, LV_ALIGN_CENTER, 1, 0);
  lv_obj_set_style_bg_color(g_rail_mid, lv_color_hex(kColLine), 0);
  lv_obj_set_style_bg_opa(g_rail_mid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_rail_mid, 0, 0);
  lv_obj_set_style_radius(g_rail_mid, 0, 0);
  lv_obj_set_style_pad_all(g_rail_mid, 0, 0);
  lv_obj_clear_flag(g_rail_mid, LV_OBJ_FLAG_SCROLLABLE);

  g_mode_lbl = lv_label_create(g_scr);
  lv_obj_set_style_text_font(g_mode_lbl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(g_mode_lbl, lv_color_hex(kColWhite), 0);
  lv_obj_set_style_text_letter_space(g_mode_lbl, 2, 0);
  lv_label_set_text(g_mode_lbl, "TIMER");
  lv_obj_align(g_mode_lbl, LV_ALIGN_TOP_LEFT, 12, 4);

  // Compact fuel gauge + % — top-right of content area (left of hint rail).
  g_batt_row = lv_obj_create(g_scr);
  lv_obj_set_size(g_batt_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(g_batt_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_batt_row, 0, 0);
  lv_obj_set_style_pad_all(g_batt_row, 0, 0);
  lv_obj_set_flex_flow(g_batt_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(g_batt_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(g_batt_row, 4, 0);
  lv_obj_clear_flag(g_batt_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(g_batt_row, LV_ALIGN_TOP_RIGHT, -(kRailW + 6), 8);

  g_batt_body = lv_obj_create(g_batt_row);
  lv_obj_set_size(g_batt_body, kBattBodyW, kBattBodyH);
  lv_obj_set_style_bg_color(g_batt_body, lv_color_hex(kColBg), 0);
  lv_obj_set_style_bg_opa(g_batt_body, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_batt_body, 1, 0);
  lv_obj_set_style_border_color(g_batt_body, lv_color_hex(kColSoft), 0);
  lv_obj_set_style_radius(g_batt_body, 2, 0);
  lv_obj_set_style_pad_all(g_batt_body, kBattPad, 0);
  lv_obj_clear_flag(g_batt_body, LV_OBJ_FLAG_SCROLLABLE);

  g_batt_fill = lv_obj_create(g_batt_body);
  lv_obj_set_size(g_batt_fill, 0, kBattBodyH - (kBattPad * 2));
  lv_obj_set_style_bg_color(g_batt_fill, lv_color_hex(kColSoft), 0);
  lv_obj_set_style_bg_opa(g_batt_fill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_batt_fill, 0, 0);
  lv_obj_set_style_radius(g_batt_fill, 1, 0);
  lv_obj_set_style_pad_all(g_batt_fill, 0, 0);
  lv_obj_align(g_batt_fill, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_clear_flag(g_batt_fill, LV_OBJ_FLAG_SCROLLABLE);

  g_batt_tip = lv_obj_create(g_batt_row);
  lv_obj_set_size(g_batt_tip, kBattTipW, kBattTipH);
  lv_obj_set_style_bg_color(g_batt_tip, lv_color_hex(kColSoft), 0);
  lv_obj_set_style_bg_opa(g_batt_tip, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_batt_tip, 0, 0);
  lv_obj_set_style_radius(g_batt_tip, 1, 0);
  lv_obj_set_style_pad_all(g_batt_tip, 0, 0);
  lv_obj_clear_flag(g_batt_tip, LV_OBJ_FLAG_SCROLLABLE);

  g_batt_lbl = lv_label_create(g_batt_row);
  lv_obj_set_style_text_font(g_batt_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_batt_lbl, lv_color_hex(kColSoft), 0);
  lv_label_set_text(g_batt_lbl, "--%");

  g_status_lbl = lv_label_create(g_scr);
  lv_obj_set_style_text_font(g_status_lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(g_status_lbl, lv_color_hex(kColMuted), 0);
  lv_obj_set_style_text_letter_space(g_status_lbl, 2, 0);
  lv_label_set_text(g_status_lbl, "READY");
  lv_obj_align(g_status_lbl, LV_ALIGN_BOTTOM_LEFT, 12, -6);

  g_hint_top = lv_label_create(g_rail);
  style_hint(g_hint_top);
  lv_label_set_text(g_hint_top, "+30\n\nMODE");
  lv_obj_align(g_hint_top, LV_ALIGN_TOP_MID, 1, 14);

  g_hint_bot = lv_label_create(g_rail);
  style_hint(g_hint_bot);
  lv_label_set_text(g_hint_bot, "START\n\nRESET");
  lv_obj_align(g_hint_bot, LV_ALIGN_BOTTOM_MID, 1, -14);

  g_time_row = lv_obj_create(g_scr);
  lv_obj_set_size(g_time_row, DISPLAY_WIDTH - kRailW, 70);
  lv_obj_align(g_time_row, LV_ALIGN_LEFT_MID, 0, 2);
  lv_obj_set_style_bg_opa(g_time_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_time_row, 0, 0);
  lv_obj_set_style_pad_all(g_time_row, 0, 0);
  lv_obj_set_flex_flow(g_time_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(g_time_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(g_time_row, 0, 0);
  lv_obj_clear_flag(g_time_row, LV_OBJ_FLAG_SCROLLABLE);

  g_d[0] = make_fixed_label(g_time_row, g_digit_w, kColGreen, "0");
  g_d[1] = make_fixed_label(g_time_row, g_digit_w, kColGreen, "0");
  g_colon[0] = make_fixed_label(g_time_row, g_colon_w, kColRed, ":");
  g_d[2] = make_fixed_label(g_time_row, g_digit_w, kColRed, "0");
  g_d[3] = make_fixed_label(g_time_row, g_digit_w, kColRed, "0");
  g_colon[1] = make_fixed_label(g_time_row, g_colon_w, kColRed, ":");
  g_d[4] = make_fixed_label(g_time_row, g_digit_w, kColRed, "0");
  g_d[5] = make_fixed_label(g_time_row, g_digit_w, kColRed, "0");

  g_help_lbl = lv_label_create(g_scr);
  lv_obj_set_style_text_font(g_help_lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(g_help_lbl, lv_color_hex(kColSoft), 0);
  lv_label_set_text(g_help_lbl, "Hold bottom ~3s\nfor setup\nJoin GymTimer");
  lv_obj_align(g_help_lbl, LV_ALIGN_LEFT_MID, 16, 8);
  lv_obj_add_flag(g_help_lbl, LV_OBJ_FLAG_HIDDEN);

  lv_screen_load(g_scr);
  lv_obj_invalidate(g_scr);
  if (prev && prev != g_scr) {
    lv_obj_delete(prev);
  }

  g_ready = true;
  update_battery(0, true);
  return true;
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

  lv_obj_set_style_bg_color(g_scr, lv_color_hex(kColBg), 0);

  lv_label_set_text(g_mode_lbl,
                    snap.mode == kModeWorkout     ? "WORKOUT"
                    : snap.mode == kModeStopwatch ? "TIMER"
                                                  : "COUNT DOWN");
  lv_obj_set_style_text_color(g_mode_lbl, lv_color_hex(kColWhite), 0);

  const bool need_setup = (snap.mode == kModeWorkout && !snap.workout_ready);
  if (need_setup) {
    lv_label_set_text(g_status_lbl, "HOLD 3s");
    lv_obj_set_style_text_color(g_status_lbl, lv_color_hex(kColAmber), 0);
    lv_obj_add_flag(g_time_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_help_lbl, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(g_status_lbl, phase_label(snap.phase));
    lv_color_t status_c = lv_color_hex(kColMuted);
    if (snap.phase == kPhaseRunning) status_c = lv_color_hex(kColGreen);
    else if (snap.phase == kPhasePaused) status_c = lv_color_hex(kColAmber);
    else if (snap.phase == kPhaseDone) status_c = lv_color_hex(kColRed);
    lv_obj_set_style_text_color(g_status_lbl, status_c, 0);
    lv_obj_clear_flag(g_time_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_help_lbl, LV_OBJ_FLAG_HIDDEN);
  }

  update_hints(snap);
  update_battery(now_ms, false);

  const uint32_t cs_total = snap.display_ms / 10u;
  if (cs_total == g_last_drawn_cs && snap.phase == g_last_phase && g_flash_on == g_last_flash_lit &&
      (uint8_t)snap.mode == g_last_mode && snap.workout_ready == g_last_wo_ready) {
    return;
  }
  g_last_drawn_cs = cs_total;
  g_last_phase = (uint8_t)snap.phase;
  g_last_mode = (uint8_t)snap.mode;
  g_last_wo_ready = snap.workout_ready;
  g_last_flash_lit = g_flash_on;

  if (need_setup) {
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
    set_time_colors(lv_color_hex(0x0A4A2A), lv_color_hex(0x4A1010));
  } else if (snap.warning_last10) {
    set_time_colors(lv_color_hex(kColRed), lv_color_hex(kColRed));
  } else if (snap.phase == kPhasePaused) {
    set_time_colors(lv_color_hex(kColAmber), lv_color_hex(kColAmber));
  } else {
    set_time_colors(lv_color_hex(kColGreen), lv_color_hex(kColRed));
  }
}

void timer_ui_show(void) {
  if (g_scr) {
    lv_screen_load(g_scr);
    lv_obj_invalidate(g_scr);
  }
}
