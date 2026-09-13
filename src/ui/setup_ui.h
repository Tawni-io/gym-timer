#pragma once

#include <stdbool.h>

bool setup_ui_init(void);
void setup_ui_show_menu(const char* profile_line);
void setup_ui_show_ap(const char* ssid, const char* ip);
void setup_ui_set_status(const char* line);
