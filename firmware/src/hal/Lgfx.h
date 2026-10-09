#pragma once

#include <LovyanGFX.hpp>

#include "config.h"
#include "hal/BacklightAw9364.h"

// ST7789 on the ESP32-S3 i80 (8-bit parallel) bus, as wired on the T-Display-S3.
// Values follow LILYGO's own LovyanGFX examples (examples/T-Display-S3-Queue).
class Lgfx : public lgfx::LGFX_Device {
 public:
  Lgfx() {
    {
      auto c = bus_.config();
      c.freq_write = cfg::lcd::kWriteClockHz;
      c.pin_wr = cfg::pin::kLcdWr;
      c.pin_rd = cfg::pin::kLcdRd;
      c.pin_rs = cfg::pin::kLcdDc;
      c.pin_d0 = cfg::pin::kLcdD0;
      c.pin_d1 = cfg::pin::kLcdD1;
      c.pin_d2 = cfg::pin::kLcdD2;
      c.pin_d3 = cfg::pin::kLcdD3;
      c.pin_d4 = cfg::pin::kLcdD4;
      c.pin_d5 = cfg::pin::kLcdD5;
      c.pin_d6 = cfg::pin::kLcdD6;
      c.pin_d7 = cfg::pin::kLcdD7;
      bus_.config(c);
      panel_.setBus(&bus_);
    }
    {
      auto c = panel_.config();
      c.pin_cs = cfg::pin::kLcdCs;
      c.pin_rst = cfg::pin::kLcdReset;
      c.pin_busy = -1;
      c.panel_width = cfg::lcd::kNativeWidth;
      c.panel_height = cfg::lcd::kNativeHeight;
      c.offset_x = cfg::lcd::kColumnOffset;
      c.offset_y = 0;
      c.offset_rotation = 1;  // so that setRotation(0) is landscape
      c.readable = false;
      c.invert = true;
      c.rgb_order = false;
      c.dlen_16bit = false;
      c.bus_shared = false;
      panel_.config(c);
    }
    panel_.setLight(&light_);
    setPanel(&panel_);
  }

 private:
  lgfx::Bus_Parallel8 bus_;
  lgfx::Panel_ST7789 panel_;
  BacklightAw9364 light_{cfg::pin::kLcdBacklight};
};
