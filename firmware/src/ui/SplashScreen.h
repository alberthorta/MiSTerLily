#pragma once

#include "ui/DisplayManager.h"

namespace SplashScreen {

// `hint` (may be null) is a status line, e.g. "Waiting for MiSTer".
void draw(DisplayManager& display, const char* version, const char* hint = nullptr);

}  // namespace SplashScreen
