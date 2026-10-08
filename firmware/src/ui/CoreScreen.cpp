#include "ui/CoreScreen.h"

#include "CoreNames.h"
#include "ui/LegacyAssetRenderer.h"
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

// Artwork layout: 256x64 picture, then the name (at most 2x, 16 px per line).
constexpr int kArtTop = 18;
constexpr int kArtNameTop = kArtTop + LegacyAssetRenderer::kHeight + 14;
constexpr int kArtNameHeight = 40;
constexpr int kArtNameMaxScale = 2;

bool sameIgnoringCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'a' && x <= 'z') x = static_cast<char>(x - 'a' + 'A');
    if (y >= 'a' && y <= 'z') y = static_cast<char>(y - 'a' + 'A');
    if (x != y) return false;
  }
  return true;
}

// The real core name, small, when the big name is a different (friendly) one.
void drawCoreNameFootnote(lgfx::LovyanGFX& g, std::string_view name, std::string_view coreName, int w, int h) {
  if (sameIgnoringCase(name, coreName)) return;
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

  drawCoreNameFootnote(g, name, coreName, w, h);
  display.present();
}

void drawWithArtwork(DisplayManager& display, std::string_view coreName, proto::ImageFormat format,
                     const uint8_t* data, size_t length) {
  auto& g = display.canvas();
  const int w = display.width();
  const int h = display.height();
  g.fillScreen(theme::kBackground);

  const int artX = (w - LegacyAssetRenderer::kWidth) / 2;
  if (!LegacyAssetRenderer::draw(g, format, data, length, artX, kArtTop, theme::kArtwork, theme::kBackground)) {
    drawNameOnly(display, coreName);
    return;
  }

  const std::string_view name = coreinfo::displayName(coreName);
  const TextRenderer::Box box{kMargin, kArtNameTop, w - 2 * kMargin, kArtNameHeight};
  TextRenderer::drawFittedC64(g, name, box, theme::kText, kArtNameMaxScale);
  drawCoreNameFootnote(g, name, coreName, w, h);
  display.present();
}

}  // namespace CoreScreen
