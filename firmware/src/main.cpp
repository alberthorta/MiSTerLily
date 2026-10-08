#include <Arduino.h>

#include "app/App.h"
#include "config.h"
#include "transport/SerialTransport.h"
#include "ui/DisplayManager.h"
#include "ui/SplashScreen.h"
#include "util/Log.h"

namespace {
DisplayManager display;
SerialTransport transport;
App app(display, transport);
}  // namespace

void setup() {
  LOG_BEGIN();
  LOGF("MiSTerLily %s boot, reset reason %d", MDS_FW_VERSION, (int)esp_reset_reason());

  transport.begin();

  // Without a panel there is nothing to show, but the serial side keeps working.
  if (display.begin()) {
    SplashScreen::draw(display, "v" MDS_FW_VERSION);
    display.setBrightness(cfg::kDefaultBrightness);
  }
  app.begin();
}

void loop() {
  app.loop();
  delay(1);
}
