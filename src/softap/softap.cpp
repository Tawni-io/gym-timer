#include "softap/softap.h"

#include "config/timer_prefs.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef TAWNI_GYM_VERSION
#define TAWNI_GYM_VERSION "0.1.0"
#endif

namespace {

WebServer g_server(80);
DNSServer g_dns;
bool g_active = false;
bool g_stop_req = false;
bool g_cfg_changed = false;
char g_ip[16] = "";

void send_flash_chunks(PGM_P s) {
  if (!s) return;
  char buf[192];
  const size_t len = strlen_P(s);
  size_t fed = 0;
  for (size_t off = 0; off < len;) {
    size_t n = len - off;
    if (n > sizeof(buf)) n = sizeof(buf);
    memcpy_P(buf, s + off, n);
    g_server.chunkWrite(buf, n);
    off += n;
    fed += n;
    if (fed >= 512) {
      fed = 0;
      delay(0);
      yield();
    }
  }
}

void send_ram_chunk(const char* s) {
  if (s && s[0]) {
    g_server.chunkWrite(s, strlen(s));
  }
}

uint16_t arg_u16(const char* name, uint16_t fallback) {
  if (!g_server.hasArg(name)) return fallback;
  const int v = g_server.arg(name).toInt();
  if (v < 0) return 0;
  if (v > 65535) return 65535;
  return (uint16_t)v;
}

void send_page(const char* flash_msg, bool flash_ok) {
  const TimerPrefs prefs = timer_prefs_load();
  const WorkoutProfile wo = timer_workout_clamp(prefs.workout);

  Serial.printf("SoftAP page stream (heap %u maxblk %u)\n", (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());

  g_server.chunkResponseBegin("text/html");
  delay(0);

  send_flash_chunks(PSTR(
      "<!DOCTYPE html><html><head><meta charset=utf-8>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<title>GymTimer</title><style>"
      "body{font-family:system-ui,sans-serif;background:#0B0F14;color:#E8EEF4;"
      "margin:0;padding:16px;max-width:480px}"
      "h1{font-size:1.25rem;margin:0 0 4px}"
      "h2{font-size:1rem;margin:20px 0 8px}"
      ".muted{color:#8B98A8;font-size:.9rem;margin:0 0 16px}"
      "label{display:block;margin:12px 0 4px;color:#8B98A8;font-size:.85rem}"
      "input,select{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;"
      "border:1px solid #2A3544;background:#151C26;color:#E8EEF4;font-size:1rem}"
      ".row{display:flex;gap:8px}.row>div{flex:1}"
      "button,.btn{display:inline-block;margin:8px 8px 0 0;padding:12px 16px;"
      "border:0;border-radius:6px;font-size:1rem;cursor:pointer;text-decoration:none}"
      ".primary{background:#3DDC97;color:#0B0F14;font-weight:600}"
      ".secondary{background:#2A3544;color:#E8EEF4}"
      ".ok{color:#3DDC97}.err{color:#F05152}"
      "hr{border:0;border-top:1px solid #2A3544;margin:20px 0}"
      "</style></head><body>"
      "<h1>GymTimer setup</h1>"));

  {
    char line[96];
    snprintf(line, sizeof(line), "<p class=muted>v" TAWNI_GYM_VERSION " · %s</p>",
             prefs.workout_ready ? "WORKOUT ready" : "No workout yet");
    send_ram_chunk(line);
  }

  if (flash_msg && flash_msg[0]) {
    send_ram_chunk(flash_ok ? "<p class=ok>" : "<p class=err>");
    send_ram_chunk(flash_msg);
    send_ram_chunk("</p>");
  }

  send_flash_chunks(PSTR(
      "<h2>Workout</h2>"
      "<p class=muted>N sets of work, rest between sets. No rest after the last set.</p>"
      "<form method=post action=/save>"));

  {
    char form[512];
    snprintf(form, sizeof(form),
             "<label>Sets (1-20)</label><input name=sets type=number min=1 max=20 value=%u>"
             "<div class=row><div><label>Work min</label>"
             "<input name=wmin type=number min=0 max=30 value=%u></div>"
             "<div><label>Work sec</label>"
             "<input name=wsec type=number min=0 max=59 value=%u></div></div>"
             "<div class=row><div><label>Rest min</label>"
             "<input name=rmin type=number min=0 max=10 value=%u></div>"
             "<div><label>Rest sec</label>"
             "<input name=rsec type=number min=0 max=59 value=%u></div></div>",
             (unsigned)wo.sets, (unsigned)(wo.work_sec / 60), (unsigned)(wo.work_sec % 60),
             (unsigned)(wo.rest_sec / 60), (unsigned)(wo.rest_sec % 60));
    send_ram_chunk(form);
  }

  send_flash_chunks(PSTR(
      "<p class=muted>This will kick you off the Access Point once it is saved.</p>"
      "<button class=primary type=submit>Save and Exit</button></form>"
      "</body></html>"));

  g_server.chunkResponseEnd();
}

void handle_root() {
  send_page(nullptr, true);
}

void handle_save() {
  WorkoutProfile p = {};
  p.sets = (uint8_t)arg_u16("sets", 5);
  const uint16_t wmin = arg_u16("wmin", 2);
  const uint16_t wsec = arg_u16("wsec", 0);
  const uint16_t rmin = arg_u16("rmin", 0);
  const uint16_t rsec = arg_u16("rsec", 30);
  p.work_sec = (uint16_t)(wmin * 60u + wsec);
  p.rest_sec = (uint16_t)(rmin * 60u + rsec);
  p = timer_workout_clamp(p);
  timer_prefs_save_workout(p);
  timer_prefs_save_workout_ready(true);
  timer_prefs_save_face(true, timer_prefs_load().simple_mode);
  g_cfg_changed = true;
  g_stop_req = true;
  g_server.sendHeader(F("Location"), F("/goodbye"), true);
  g_server.send(303, "text/plain", "");
}

void handle_goodbye() {
  g_server.send(200, "text/html",
                F("<!DOCTYPE html><html><head><meta charset=utf-8>"
                  "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
                  "<title>GymTimer</title></head><body style=\"font-family:system-ui;"
                  "background:#0B0F14;color:#E8EEF4;padding:24px\">"
                  "<h1>Hotspot stopping…</h1>"
                  "<p>You can leave GymTimer Wi-Fi.</p>"
                  "</body></html>"));
}

void handle_stop() {
  g_stop_req = true;
  g_server.sendHeader(F("Location"), F("/goodbye"), true);
  g_server.send(303, "text/plain", "");
}

void handle_stop_get() {
  g_server.sendHeader(F("Location"), F("/goodbye"), true);
  g_server.send(302, "text/plain", "");
}

void handle_captive() {
  char loc[40];
  snprintf(loc, sizeof(loc), "http://%s/", g_ip[0] ? g_ip : "192.168.4.1");
  g_server.sendHeader(F("Location"), loc, true);
  g_server.send(302, "text/plain", "");
}

void handle_generate_204() {
  g_server.send(204, "text/plain", "");
}

void handle_ms_ncsi() {
  g_server.send(200, "text/plain", F("Microsoft NCSI"));
}

}  // namespace

bool softap_start(void) {
  if (g_active) return true;

  g_stop_req = false;

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  {
    const uint32_t settle_until = millis() + 600;
    while ((int32_t)(millis() - settle_until) < 0) {
      delay(40);
      if (ESP.getMaxAllocHeap() >= 24000) {
        break;
      }
    }
    delay(150);
  }
  Serial.printf("SoftAP pre-AP (heap %u maxblk %u)\n", (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());
  WiFi.mode(WIFI_AP);
  delay(80);

  bool ok = WiFi.softAP(kSoftApSsid, nullptr, 1, 0, 1);
  if (!ok) {
    Serial.println("SoftAP start FAILED — retry after WIFI_OFF");
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_AP);
    delay(80);
    ok = WiFi.softAP(kSoftApSsid, nullptr, 1, 0, 1);
  }
  if (!ok) {
    Serial.println("SoftAP start FAILED");
    WiFi.mode(WIFI_OFF);
    return false;
  }

  delay(200);
  IPAddress ip = WiFi.softAPIP();
  snprintf(g_ip, sizeof(g_ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  Serial.printf("SoftAP %s  http://%s  ch=%d  heap=%u maxblk=%u\n", kSoftApSsid, g_ip,
                WiFi.channel(), (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());

  g_dns.start(53, "*", ip);

  g_server.on("/", HTTP_GET, handle_root);
  g_server.on("/save", HTTP_POST, handle_save);
  g_server.on("/stop", HTTP_POST, handle_stop);
  g_server.on("/stop", HTTP_GET, handle_stop_get);
  g_server.on("/goodbye", HTTP_GET, handle_goodbye);
  g_server.on("/generate_204", HTTP_GET, handle_generate_204);
  g_server.on("/hotspot-detect.html", HTTP_GET, handle_captive);
  g_server.on("/library/test/success.html", HTTP_GET, handle_captive);
  g_server.on("/connecttest.txt", HTTP_GET, handle_captive);
  g_server.on("/ncsi.txt", HTTP_GET, handle_ms_ncsi);
  g_server.on("/fwlink", HTTP_GET, handle_captive);
  g_server.onNotFound(handle_captive);
  g_server.begin();

  g_active = true;
  return true;
}

void softap_stop(void) {
  if (!g_active) return;
  g_server.stop();
  g_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  g_active = false;
  g_stop_req = false;
  g_ip[0] = '\0';
  Serial.println("SoftAP stopped");
  delay(350);
}

void softap_loop(void) {
  if (!g_active) return;
  g_dns.processNextRequest();
  g_server.handleClient();
}

bool softap_active(void) { return g_active; }

const char* softap_ip(void) { return g_ip; }

bool softap_stop_requested(void) { return g_stop_req; }

void softap_clear_stop_request(void) { g_stop_req = false; }

bool softap_config_changed(void) { return g_cfg_changed; }

void softap_clear_config_changed(void) { g_cfg_changed = false; }
