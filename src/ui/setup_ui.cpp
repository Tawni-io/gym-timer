#include "ui/setup_ui.h"

#include <stdio.h>
#include <lvgl.h>

#include "board_pins.h"

namespace {

constexpr uint32_t kBg = 0x0D1117;
constexpr uint32_t kCard = 0x161B22;
constexpr uint32_t kFg = 0xFFFFFF;
constexpr uint32_t kMuted = 0x8B949E;
constexpr uint32_t kOk = 0x00E676;
constexpr uint32_t kCyan = 0x00BCD4;
constexpr uint32_t kOrange = 0xFF9100;

constexpr int kRailW = 58;
constexpr int kStripH = 3;

lv_obj_t* g_scr = nullptr;
lv_obj_t* g_title = nullptr;
lv_obj_t* g_profile = nullptr;
lv_obj_t* g_line1 = nullptr;
lv_obj_t* g_line2 = nullptr;
lv_obj_t* g_line3 = nullptr;
lv_obj_t* g_hint_top_short = nullptr;
lv_obj_t* g_hint_top_long = nullptr;
lv_obj_t* g_hint_bot_short = nullptr;
lv_obj_t* g_hint_bot_long = nullptr;
bool g_ready = false;

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

lv_obj_t* make_card(lv_obj_t* parent, int x, int y, int w, int h, uint32_t accent) {
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
  return card;
}

void style_hint_short(lv_obj_t* lbl) {
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(kFg), 0);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
}

void style_hint_long(lv_obj_t* lbl) {
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(kMuted), 0);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
}

void set_common_hints(bool ap) {
  lv_label_set_text(g_hint_top_short, ap ? "" : "MODE");
  lv_label_set_text(g_hint_top_long, "");
  lv_obj_add_flag(g_hint_top_long, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(g_hint_top_short, LV_ALIGN_CENTER, 0, 2);
  if (ap) {
    lv_label_set_text(g_hint_bot_short, "EXIT");
    lv_label_set_text(g_hint_bot_long, "");
    lv_obj_add_flag(g_hint_bot_long, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(g_hint_bot_short, LV_ALIGN_CENTER, 0, 2);
  } else {
    lv_label_set_text(g_hint_bot_short, "START");
    lv_label_set_text(g_hint_bot_long, "RESET");
    lv_obj_clear_flag(g_hint_bot_long, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(g_hint_bot_short, LV_ALIGN_TOP_MID, 0, 18);
  }
}

}  // namespace

bool setup_ui_init(void) {
  g_scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(g_scr, lv_color_hex(kBg), 0);
  lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_scr, 0, 0);
  lv_obj_set_style_pad_all(g_scr, 0, 0);
  lv_obj_set_style_radius(g_scr, 0, 0);
  lv_obj_clear_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* strip = lv_obj_create(g_scr);
  lv_obj_set_pos(strip, 0, 0);
  lv_obj_set_size(strip, DISPLAY_WIDTH, kStripH);
  strip_chrome(strip);
  lv_obj_set_style_bg_color(strip, lv_color_hex(kOk), 0);
  lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);

  lv_obj_t* card = lv_obj_create(g_scr);
  lv_obj_set_pos(card, 8, 10);
  lv_obj_set_size(card, DISPLAY_WIDTH - kRailW - 16, DISPLAY_HEIGHT - 20);
  strip_chrome(card);
  lv_obj_set_style_bg_color(card, lv_color_hex(kCard), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(card, 10, 0);

  g_title = make_label(card, &lv_font_montserrat_20, kOk);
  lv_label_set_text(g_title, "SETUP");
  lv_obj_align(g_title, LV_ALIGN_TOP_LEFT, 0, 2);

  g_profile = make_label(card, &lv_font_montserrat_14, kFg);
  lv_label_set_text(g_profile, "SIMPLE");
  lv_obj_align(g_profile, LV_ALIGN_TOP_LEFT, 0, 32);

  g_line1 = make_label(card, &lv_font_montserrat_12, kMuted);
  lv_label_set_text(g_line1, "Join on your phone");
  lv_obj_align(g_line1, LV_ALIGN_TOP_LEFT, 0, 62);

  g_line2 = make_label(card, &lv_font_montserrat_12, kCyan);
  lv_label_set_text(g_line2, "http://192.168.4.1");
  lv_obj_align(g_line2, LV_ALIGN_TOP_LEFT, 0, 82);

  g_line3 = make_label(card, &lv_font_montserrat_12, kMuted);
  lv_label_set_text(g_line3, "Hold bottom to finish");
  lv_obj_align(g_line3, LV_ALIGN_TOP_LEFT, 0, 102);

  const int rail_x = DISPLAY_WIDTH - kRailW;
  const int card_w = kRailW - 6;
  const int card_h = 72;
  lv_obj_t* top = make_card(g_scr, rail_x, 8, card_w, card_h, kCyan);
  lv_obj_t* bot = make_card(g_scr, rail_x, DISPLAY_HEIGHT - 8 - card_h, card_w, card_h, kOrange);

  g_hint_top_short = lv_label_create(top);
  style_hint_short(g_hint_top_short);
  lv_obj_align(g_hint_top_short, LV_ALIGN_TOP_MID, 0, 18);

  g_hint_top_long = lv_label_create(top);
  style_hint_long(g_hint_top_long);
  lv_obj_align(g_hint_top_long, LV_ALIGN_BOTTOM_MID, 0, -12);

  g_hint_bot_short = lv_label_create(bot);
  style_hint_short(g_hint_bot_short);
  lv_obj_align(g_hint_bot_short, LV_ALIGN_TOP_MID, 0, 18);

  g_hint_bot_long = lv_label_create(bot);
  style_hint_long(g_hint_bot_long);
  lv_obj_align(g_hint_bot_long, LV_ALIGN_BOTTOM_MID, 0, -12);

  set_common_hints(false);
  g_ready = true;
  return true;
}

void setup_ui_show_menu(const char* profile_line) {
  if (!g_ready) return;
  lv_label_set_text(g_title, "SETUP");
  lv_label_set_text(g_profile, profile_line ? profile_line : "WIFI FAILED");
  lv_label_set_text(g_line1, "Hold bottom: back");
  lv_label_set_text(g_line2, "Join GymTimer on your phone");
  lv_label_set_text(g_line3, "http://192.168.4.1");
  set_common_hints(true);
  lv_screen_load(g_scr);
  lv_obj_invalidate(g_scr);
}

void setup_ui_show_ap(const char* ssid, const char* ip) {
  if (!g_ready) return;
  lv_label_set_text(g_title, "SETUP MODE");
  char wifi[40];
  snprintf(wifi, sizeof(wifi), "Wi-Fi: %s", ssid ? ssid : "GymTimer");
  lv_label_set_text(g_profile, wifi);
  char url[40];
  snprintf(url, sizeof(url), "http://%s", (ip && ip[0]) ? ip : "192.168.4.1");
  lv_label_set_text(g_line1, "Join on your phone");
  lv_label_set_text(g_line2, url);
  lv_label_set_text(g_line3, "Hold bottom to finish");
  set_common_hints(true);
  lv_screen_load(g_scr);
  lv_obj_invalidate(g_scr);
}

void setup_ui_set_status(const char* line) {
  if (!g_ready || !line) return;
  lv_label_set_text(g_profile, line);
}
