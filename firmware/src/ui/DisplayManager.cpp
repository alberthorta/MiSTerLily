#include "ui/DisplayManager.h"

#include <Arduino.h>

#include "config.h"
#include "util/Log.h"

bool DisplayManager::begin() {
  pinMode(cfg::pin::kPowerOn, OUTPUT);
  digitalWrite(cfg::pin::kPowerOn, HIGH);

  // init() pulses LCD reset and clears the panel, so a warm reboot starts from a known state.
  // The backlight stays off until the first frame is pushed.
  if (!lcd_.init()) {
    LOGF("lcd init failed");
    return false;
  }
  lcd_.setBrightness(0);
  lcd_.setRotation(cfg::kRotationNormal);
  lcd_.fillScreen(TFT_BLACK);

  frame_.setColorDepth(16);
  frame_.setPsram(true);
  framebufferOk_ = frame_.createSprite(cfg::kScreenWidth, cfg::kScreenHeight) != nullptr;
  // Without PSRAM we still draw, straight to the panel (with some flicker).
  LOGF("lcd ok %dx%d, framebuffer %s", lcd_.width(), lcd_.height(), framebufferOk_ ? "psram" : "none");
  return true;
}

lgfx::LovyanGFX& DisplayManager::canvas() {
  if (framebufferOk_) return frame_;
  return lcd_;
}

void DisplayManager::present() {
  // While asleep the frame stays in the buffer; wake() pushes it.
  if (framebufferOk_ && awake_) frame_.pushSprite(0, 0);
}

void DisplayManager::setBrightness(uint8_t level) {
  brightness_ = level;
  if (awake_) lcd_.setBrightness(level);
}

void DisplayManager::setRotation(uint8_t rotation) {
  lcd_.setRotation(rotation);
  present();
}

void DisplayManager::sleep() {
  if (!awake_) return;
  awake_ = false;
  lcd_.setBrightness(0);
  lcd_.sleep();
}

void DisplayManager::wake() {
  if (awake_) return;
  awake_ = true;
  lcd_.wakeup();
  present();
  lcd_.setBrightness(brightness_);
}

int DisplayManager::width() const { return cfg::kScreenWidth; }

int DisplayManager::height() const { return cfg::kScreenHeight; }

const uint8_t* DisplayManager::framebufferBytes() const {
  return framebufferOk_ ? static_cast<const uint8_t*>(const_cast<LGFX_Sprite&>(frame_).getBuffer()) : nullptr;
}

size_t DisplayManager::framebufferSize() const {
  return framebufferOk_ ? static_cast<size_t>(cfg::kScreenWidth) * cfg::kScreenHeight * 2 : 0;
}
