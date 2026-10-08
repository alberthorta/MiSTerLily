#pragma once

#include <string_view>

#include "ui/DisplayManager.h"

namespace CoreScreen {

// Text-only core screen: the system name, large and centered. When the friendly name differs
// from the MiSTer core name (PSX -> PlayStation), the core name is shown small underneath.
void drawNameOnly(DisplayManager& display, std::string_view coreName);

}  // namespace CoreScreen
