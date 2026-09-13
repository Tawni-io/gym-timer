#pragma once

// Thin wrapper over LilyGO board_config — pin source of truth for this project.
// Upstream: https://github.com/Xinyuan-LilyGO/T-Display-C5/blob/master/include/board_config.h

#include "board_config.h"

// Landscape UI (native panel defs are portrait 170×320)
#define DISPLAY_WIDTH  LCD_HEIGHT  // 320
#define DISPLAY_HEIGHT LCD_WIDTH   // 170
