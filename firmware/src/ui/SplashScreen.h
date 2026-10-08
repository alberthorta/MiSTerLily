#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ui/DisplayManager.h"

namespace SplashScreen {

// A stored start-up picture (PNG or JPEG), drawn centered in the top 320x130 area.
struct Logo {
  const uint8_t* data = nullptr;
  size_t size = 0;
  bool png = true;
};

constexpr int kLogoAreaHeight = 130;

// Start screen: the logo if there is one (else "MiSTer / T-Display-S3" text), the firmware version
// and `hint` (may be null), a status line such as "Waiting for MiSTer".
void draw(DisplayManager& display, const char* version, const char* hint = nullptr, const Logo* logo = nullptr);

}  // namespace SplashScreen
