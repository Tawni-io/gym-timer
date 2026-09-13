#pragma once

#include "timer/timer_engine.h"

bool timer_ui_init(void);
void timer_ui_show(void);
void timer_ui_update(const TimerSnapshot& snap, uint32_t now_ms);
