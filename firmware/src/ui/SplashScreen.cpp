#include "ui/SplashScreen.h"

#include "ui/Theme.h"

namespace SplashScreen {

namespace {

// The hint is Font2 at 2x (32 px). It fills most of the line below the logo, so the version moved
// to the top-right corner, which the logo area leaves free.
constexpr int kHintY = 149;
constexpr float kHintScale = 2.0f;

bool drawLogo(lgfx::LovyanGFX& g, int w, const Logo& logo) {
  if (!logo.data || logo.size == 0) return false;
  // The datum centres the picture in the (0, 0, w, kLogoAreaHeight) box; larger ones are cropped.
  return logo.png ? g.drawPng(logo.data, logo.size, 0, 0, w, kLogoAreaHeight, 0, 0, 1.0f, 0.0f, lgfx::middle_center)
                  : g.drawJpg(logo.data, logo.size, 0, 0, w, kLogoAreaHeight, 0, 0, 1.0f, 0.0f, lgfx::middle_center);
}

void drawTextTitle(lgfx::LovyanGFX& g, int w) {
  g.setTextDatum(lgfx::middle_center);
  g.setTextColor(theme::kText);
  g.setFont(&fonts::FreeSansBold24pt7b);
  g.drawString("MiSTer", w / 2, 62);

  g.fillRect(w / 2 - 50, 92, 100, 3, theme::kAccent);

  g.setTextColor(theme::kTextDim);
  g.setFont(&fonts::FreeSans12pt7b);
  g.drawString("T-Display-S3", w / 2, 118);
}

}  // namespace

void draw(DisplayManager& display, const char* version, const char* hint, const Logo* logo) {
  auto& g = display.canvas();
  const int w = display.width();

  g.fillScreen(theme::kBackground);
  g.setTextSize(1);
  if (!logo || !drawLogo(g, w, *logo)) {
    g.fillScreen(theme::kBackground);  // a failed decode may have drawn part of the picture
    drawTextTitle(g, w);
  }

  g.setTextDatum(lgfx::top_right);
  g.setTextColor(theme::kTextFaint);
  g.setFont(&fonts::Font2);
  g.drawString(version, w - 4, 2);

  if (hint) {
    g.setTextDatum(lgfx::middle_center);
    g.setTextColor(theme::kAccent);
    g.setTextSize(kHintScale);
    g.drawString(hint, w / 2, kHintY);
    g.setTextSize(1);
  }

  display.present();
}

}  // namespace SplashScreen
