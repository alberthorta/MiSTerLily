#pragma once

#include <stdint.h>

#include "hal/Lgfx.h"

// Owns the panel and a full-screen RGB565 framebuffer. Screens draw into canvas() and call
// present(), which pushes the whole frame in one transfer (no visible partial redraws).
class DisplayManager {
 public:
  bool begin();

  lgfx::LovyanGFX& canvas();
  void present();

  // Remembered while asleep: wake() restores it.
  void setBrightness(uint8_t level);
  void setRotation(uint8_t rotation);
  void sleep();
  void wake();
  bool awake() const { return awake_; }

  int width() const;
  int height() const;
  bool hasFramebuffer() const { return framebufferOk_; }

 private:
  Lgfx lcd_;
  LGFX_Sprite frame_{&lcd_};
  bool framebufferOk_ = false;
  bool awake_ = true;
  uint8_t brightness_ = 0;
};
