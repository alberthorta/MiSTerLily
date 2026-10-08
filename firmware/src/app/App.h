#pragma once

#include <stdint.h>

#include <string_view>

#include "Command.h"
#include "ProtocolParser.h"
#include "transport/SerialTransport.h"
#include "ui/DisplayManager.h"

// Glue between the serial protocol and the screens. The parser only produces events; this class
// keeps the state (current core, its picture, display settings) and decides what to draw.
class App : public proto::ParserListener {
 public:
  App(DisplayManager& display, SerialTransport& transport);

  void begin();
  void loop();

  // ParserListener
  void onLine(const char* line, size_t length) override;
  void onImage(const proto::ImageFrame& frame) override;
  void onDiscard(proto::DiscardReason reason, size_t length) override;

 private:
  static constexpr size_t kMaxCoreName = 63;

  void handle(const proto::Command& cmd, std::string_view line);
  void setCore(std::string_view name);
  void showCore();
  void showSplash();
  void setContrast(std::string_view args);
  void setRotation(std::string_view args);
  void clearWithFill(std::string_view args);
  void sendHardwareInfo();
  void sendInfo();
  void ack();
  void startScreenshot();
  void pumpScreenshot(uint32_t nowMs);

  DisplayManager& display_;
  SerialTransport& transport_;
  proto::ProtocolParser parser_;

  char coreName_[kMaxCoreName + 1] = {};
  size_t coreNameLength_ = 0;
  // The last received picture (CMDCOR/CMDAPD), kept for CMDSPIC.
  uint8_t picture_[proto::kImageBufferBytes] = {};
  proto::ImageFormat pictureFormat_ = proto::ImageFormat::None;

  // Screenshot being streamed out (MDSSHOT), a little per loop.
  const uint8_t* shotData_ = nullptr;
  size_t shotLeft_ = 0;
  uint32_t shotProgressMs_ = 0;

  bool sendTtyAck_ = true;
  bool restartPending_ = false;
  uint32_t restartAtMs_ = 0;

  uint32_t linesReceived_ = 0;
  uint32_t imagesReceived_ = 0;
  uint32_t discards_ = 0;
  proto::ImageResult lastImageResult_ = proto::ImageResult::NoData;
  size_t lastImageBytes_ = 0;
};
