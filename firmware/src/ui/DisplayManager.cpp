#include "ui/DisplayManager.h"

#include <Arduino.h>

#include "config.h"
#include "util/Log.h"

namespace {

// Short enough that a core change still feels immediate, long enough to read as a fade.
constexpr uint32_t kFadeOutMs = 120;
constexpr uint32_t kFadeInMs = 180;

}  // namespace

bool DisplayManager::begin(uint8_t rotation) {
  pinMode(cfg::pin::kPowerOn, OUTPUT);
  digitalWrite(cfg::pin::kPowerOn, HIGH);

  // init() pulses LCD reset and clears the panel, so a warm reboot starts from a known state.
  // The backlight stays off until the first frame is pushed.
  if (!lcd_.init()) {
    LOGF("lcd init failed");
    return false;
  }
  lcd_.setBrightness(0);
  lcd_.setRotation(rotation);
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
  if (fadeArmed_) {
    fadeArmed_ = false;
    presentFaded(fadeArmedMs_);
    return;
  }
  // While asleep the frame stays in the buffer; wake() pushes it. A plain present() cancels a
  // fade (the newest frame wins, shown at once).
  if (fade_ != Fade::None) finishFade();
  if (framebufferOk_ && awake_) frame_.pushSprite(0, 0);
}

void DisplayManager::presentFaded(uint32_t nowMs) {
  if (!framebufferOk_ || !awake_ || !backlightOn_) {
    finishFade();
    if (framebufferOk_ && awake_) frame_.pushSprite(0, 0);
    return;
  }
  switch (fade_) {
    case Fade::None:
      fade_ = Fade::Out;
      fadeStartMs_ = nowMs;
      fadeFrom_ = brightness_;
      level_ = brightness_;
      break;
    case Fade::Out:
      break;  // the new frame is already in the buffer; it is pushed at the bottom of the dip
    case Fade::In:
      // A new frame while coming back up: go down again from where we are.
      fade_ = Fade::Out;
      fadeStartMs_ = nowMs;
      fadeFrom_ = level_;
      break;
  }
}

void DisplayManager::tick(uint32_t nowMs) {
  if (fade_ == Fade::None) return;
  const uint32_t t = nowMs - fadeStartMs_;
  if (fade_ == Fade::Out) {
    if (t >= kFadeOutMs) {
      level_ = 0;
      lcd_.setBrightness(0);
      frame_.pushSprite(0, 0);
      fade_ = Fade::In;
      fadeStartMs_ = nowMs;
      return;
    }
    level_ = static_cast<uint8_t>(fadeFrom_ - fadeFrom_ * t / kFadeOutMs);
  } else {
    if (t >= kFadeInMs) {
      fade_ = Fade::None;
      lcd_.setBrightness(brightness_);
      return;
    }
    level_ = static_cast<uint8_t>(brightness_ * t / kFadeInMs);
  }
  lcd_.setBrightness(level_);
}

// Ends a fade at once: the pending frame is shown and the light restored (if it should be on).
void DisplayManager::finishFade() {
  if (fade_ == Fade::None) return;
  if (fade_ == Fade::Out && framebufferOk_ && awake_) frame_.pushSprite(0, 0);
  fade_ = Fade::None;
  if (awake_) lcd_.setBrightness(backlightOn_ ? brightness_ : 0);
}

void DisplayManager::setBrightness(uint8_t level) {
  brightness_ = level;
  // During a fade tick() owns the backlight and ends at brightness_.
  if (awake_ && backlightOn_ && fade_ == Fade::None) lcd_.setBrightness(level);
}

void DisplayManager::setBacklightOn(bool on) {
  backlightOn_ = on;
  finishFade();
  if (awake_) lcd_.setBrightness(on ? brightness_ : 0);
}

void DisplayManager::setRotation(uint8_t rotation) {
  lcd_.setRotation(rotation);
  present();
}

void DisplayManager::sleep() {
  if (!awake_) return;
  finishFade();
  awake_ = false;
  lcd_.setBrightness(0);
  lcd_.sleep();
}

void DisplayManager::wake() {
  if (awake_) return;
  awake_ = true;
  lcd_.wakeup();
  present();
  if (backlightOn_) lcd_.setBrightness(brightness_);
}

int DisplayManager::width() const { return cfg::kScreenWidth; }

int DisplayManager::height() const { return cfg::kScreenHeight; }

const uint8_t* DisplayManager::framebufferBytes() const {
  return framebufferOk_ ? static_cast<const uint8_t*>(const_cast<LGFX_Sprite&>(frame_).getBuffer()) : nullptr;
}

size_t DisplayManager::framebufferSize() const {
  return framebufferOk_ ? static_cast<size_t>(cfg::kScreenWidth) * cfg::kScreenHeight * 2 : 0;
}
