#include "ui/CoreScreen.h"

#include "CoreNames.h"
#include "ui/TextRenderer.h"
#include "ui/Theme.h"

namespace CoreScreen {

namespace {

constexpr int kMargin = 12;
constexpr int kNameTop = 30;
constexpr int kNameHeight = 80;
constexpr int kAccentWidth = 64;
constexpr int kAccentHeight = 3;
constexpr int kAccentGap = 10;

}  // namespace

void drawNameOnly(DisplayManager& display, std::string_view coreName) {
  auto& g = display.canvas();
  const int w = display.width();
  const int h = display.height();
  g.fillScreen(theme::kBackground);

  const std::string_view name = coreinfo::displayName(coreName);
  const TextRenderer::Box box{kMargin, kNameTop, w - 2 * kMargin, kNameHeight};
  const int bottom = TextRenderer::drawFittedC64(g, name, box, theme::kText);

  const int accentY = bottom + kAccentGap;
  g.fillRect((w - kAccentWidth) / 2, accentY, kAccentWidth, kAccentHeight, theme::kAccent);

  if (name.data() != coreName.data()) {
    // drawString needs a terminated string; core names are short.
    char raw[64];
    const size_t n = coreName.size() < sizeof(raw) - 1 ? coreName.size() : sizeof(raw) - 1;
    for (size_t i = 0; i < n; ++i) raw[i] = coreName[i];
    raw[n] = '\0';
    g.setFont(&fonts::Font2);
    g.setTextSize(1);
    g.setTextColor(theme::kTextFaint);
    g.setTextDatum(lgfx::bottom_center);
    g.drawString(raw, w / 2, h - 6);
  }

  display.present();
}

}  // namespace CoreScreen
