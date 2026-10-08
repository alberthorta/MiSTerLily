#include "ui/LegacyAssetRenderer.h"

namespace LegacyAssetRenderer {

namespace {

constexpr int kLevels = 16;
constexpr int kXbmRowBytes = kWidth / 8;
constexpr int kGscRowBytes = kWidth / 2;

uint16_t blend565(uint16_t from, uint16_t to, int level) {
  const int r0 = (from >> 11) & 0x1F, g0 = (from >> 5) & 0x3F, b0 = from & 0x1F;
  const int r1 = (to >> 11) & 0x1F, g1 = (to >> 5) & 0x3F, b1 = to & 0x1F;
  const int max = kLevels - 1;
  const int r = r0 + (r1 - r0) * level / max;
  const int gg = g0 + (g1 - g0) * level / max;
  const int b = b0 + (b1 - b0) * level / max;
  return static_cast<uint16_t>((r << 11) | (gg << 5) | b);
}

}  // namespace

bool draw(lgfx::LovyanGFX& g, proto::ImageFormat format, const uint8_t* data, size_t length, int x, int y,
          uint16_t foreground, uint16_t background) {
  if (format == proto::ImageFormat::Xbm && length != proto::kXbmBytes) return false;
  if (format == proto::ImageFormat::Gsc && length != proto::kGscBytes) return false;
  if (format == proto::ImageFormat::None) return false;

  uint16_t palette[kLevels];
  for (int i = 0; i < kLevels; ++i) palette[i] = blend565(background, foreground, i);

  g.startWrite();
  for (int row = 0; row < kHeight; ++row) {
    if (format == proto::ImageFormat::Xbm) {
      const uint8_t* src = data + row * kXbmRowBytes;
      for (int col = 0; col < kWidth; ++col) {
        const bool on = (src[col >> 3] >> (col & 7)) & 1;
        g.writePixel(x + col, y + row, on ? foreground : background);
      }
    } else {
      const uint8_t* src = data + row * kGscRowBytes;
      for (int col = 0; col < kWidth; ++col) {
        const uint8_t b = src[col >> 1];
        const int level = (col & 1) ? (b & 0x0F) : (b >> 4);
        g.writePixel(x + col, y + row, palette[level]);
      }
    }
  }
  g.endWrite();
  return true;
}

}  // namespace LegacyAssetRenderer
