#pragma once

#include <stdbool.h>

constexpr const char* kSoftApSsid = "GymTimer";

bool softap_start(void);
void softap_stop(void);
void softap_loop(void);
bool softap_active(void);

const char* softap_ip(void);

bool softap_stop_requested(void);
void softap_clear_stop_request(void);

bool softap_config_changed(void);
void softap_clear_config_changed(void);
