#include <Arduino.h>

#include "config.h"
#include "ui/DisplayManager.h"
#include "ui/SplashScreen.h"
#include "util/Log.h"

namespace {
DisplayManager display;
}

void setup() {
  LOG_BEGIN();
  LOGF("MiSTerLily %s boot, reset reason %d", MDS_FW_VERSION, (int)esp_reset_reason());

  if (!display.begin()) {
    // No panel means nothing useful to show; keep running so USB stays reachable.
    return;
  }
  SplashScreen::draw(display, "v" MDS_FW_VERSION);
  display.setBrightness(cfg::kDefaultBrightness);
}

void loop() { delay(10); }
