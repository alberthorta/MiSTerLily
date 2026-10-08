#pragma once

#include <stdint.h>

#include "Command.h"
#include "ProtocolParser.h"
#include "transport/SerialTransport.h"
#include "ui/DisplayManager.h"

// Glue between the serial protocol and the screens. The parser only produces events; this class
// decides what each one means and what to draw.
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
  void handle(const proto::Command& cmd);
  void sendHardwareInfo();
  void sendInfo();
  void ack();

  DisplayManager& display_;
  SerialTransport& transport_;
  proto::ProtocolParser parser_;

  bool sendTtyAck_ = true;
  bool restartPending_ = false;
  uint32_t restartAtMs_ = 0;

  uint32_t linesReceived_ = 0;
  uint32_t imagesReceived_ = 0;
  uint32_t discards_ = 0;
  proto::ImageResult lastImageResult_ = proto::ImageResult::NoData;
  size_t lastImageBytes_ = 0;
};
