#include "ui/SplashScreen.h"

#include "ui/Theme.h"

namespace SplashScreen {

void draw(DisplayManager& display, const char* version) {
  auto& g = display.canvas();
  const int w = display.width();
  const int h = display.height();

  g.fillScreen(theme::kBackground);

  g.setTextDatum(lgfx::middle_center);
  g.setTextColor(theme::kText);
  g.setFont(&fonts::FreeSansBold24pt7b);
  g.drawString("MiSTer", w / 2, 62);

  g.fillRect(w / 2 - 50, 92, 100, 3, theme::kAccent);

  g.setTextColor(theme::kTextDim);
  g.setFont(&fonts::FreeSans12pt7b);
  g.drawString("T-Display-S3", w / 2, 118);

  g.setTextDatum(lgfx::bottom_right);
  g.setTextColor(theme::kTextFaint);
  g.setFont(&fonts::Font2);
  g.drawString(version, w - 6, h - 4);

  // Lets a bring-up test confirm PSRAM without a UART adapter.
  g.setTextDatum(lgfx::bottom_left);
  g.drawString(display.hasFramebuffer() ? "fb: psram" : "fb: direct", 6, h - 4);

  display.present();
}

}  // namespace SplashScreen
