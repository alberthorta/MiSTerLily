#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ProtocolParser.h"
#include "hal/Lgfx.h"

// Draws a tty2oled picture (256x64) at 1:1. Formats, as the sketch reads them:
// - XBM: 1 bpp, 32 bytes per row, least significant bit = leftmost pixel.
// - GSC: 4 bpp, 128 bytes per row, high nibble = left pixel, 0 = black .. 15 = full.
// Grey levels are blended from `background` to `foreground`, so a tint colour also works.
namespace LegacyAssetRenderer {

constexpr int kWidth = 256;
constexpr int kHeight = 64;

// False (and nothing drawn) if the format/length pair is not a valid picture.
bool draw(lgfx::LovyanGFX& g, proto::ImageFormat format, const uint8_t* data, size_t length, int x, int y,
          uint16_t foreground, uint16_t background);

}  // namespace LegacyAssetRenderer
