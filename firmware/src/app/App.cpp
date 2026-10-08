#include "app/App.h"

#include <Arduino.h>
#include <stdio.h>

#include "config.h"
#include "util/Log.h"

namespace {

constexpr size_t kReadChunkBytes = 512;
// Lets the "ttyack;" of CMDRESET leave before the chip restarts.
constexpr uint32_t kRestartDelayMs = 100;

}  // namespace

App::App(DisplayManager& display, SerialTransport& transport)
    : display_(display), transport_(transport), parser_(*this) {}

void App::begin() {
  // The sketch prints "ttyrdy;" here, but no tty2oled script reads it. Over HWCDC it stays
  // queued until the next reply and then corrupts that reply (installer.sh reads one line for
  // CMDHWINF), so we do not send it.
}

void App::loop() {
  uint8_t buffer[kReadChunkBytes];
  // Read until the FIFO is empty: while it is full the host is held off (NAK).
  for (;;) {
    const size_t n = transport_.read(buffer, sizeof(buffer));
    if (n == 0) break;
    parser_.feed(buffer, n, millis());
  }
  parser_.poll(millis());
  transport_.poll(millis());

  if (restartPending_ && static_cast<int32_t>(millis() - restartAtMs_) >= 0) {
    LOGF("restart");
    ESP.restart();
  }
}

void App::onLine(const char* line, size_t length) {
  ++linesReceived_;
  const proto::Command cmd = proto::parseCommand(std::string_view(line, length));
  LOGF("line '%s' -> %d", line, (int)cmd.id);
  handle(cmd);
  ack();
}

void App::onImage(const proto::ImageFrame& frame) {
  ++imagesReceived_;
  lastImageResult_ = frame.result;
  lastImageBytes_ = frame.length;
  LOGF("image '%s' result %d format %d bytes %u", frame.commandLine, (int)frame.result, (int)frame.format,
       (unsigned)frame.length);
  // Phase 3/4 draws it. The sketch acks CMDCOR after the picture was read.
  ack();
}

void App::onDiscard(proto::DiscardReason reason, size_t length) {
  ++discards_;
  LOGF("discard reason %d, %u bytes", (int)reason, (unsigned)length);
}

void App::handle(const proto::Command& cmd) {
  using proto::CommandId;
  switch (cmd.id) {
    case CommandId::HardwareInfo:
      sendHardwareInfo();
      break;
    case CommandId::MdsPing:
      transport_.write("MDSPONG\n");
      break;
    case CommandId::MdsInfo:
      sendInfo();
      break;
    case CommandId::SetTtyAck: {
      int32_t v = 1;
      if (proto::parseInt(cmd.args, v)) sendTtyAck_ = v != 0;
      break;
    }
    case CommandId::Reset:
      restartPending_ = true;
      restartAtMs_ = millis() + kRestartDelayMs;
      break;
    default:
      // Everything else (core names, display commands) arrives in the next phases.
      break;
  }
}

void App::sendHardwareInfo() {
  // installer.sh reads this with `read`, takes ${BLA:0:9} as the board id and ${SWver:10} as
  // the version. The id is exactly 9 characters and unknown to the tty2oled flasher; the version
  // starts with a letter, so it compares newer than any tty2oled build date and no update is offered.
  transport_.write("HWTDISPS3;MDS-" MDS_FW_VERSION ";\n");
}

void App::sendInfo() {
  char line[200];
  const int n = snprintf(line, sizeof(line),
                         "MDSINFO;fw=%s;board=T-Display-S3;psram=%u;fb=%s;uptime_ms=%lu;reset=%d;"
                         "lines=%lu;images=%lu;discards=%lu;txdrop=%lu;txstale=%lu;lastimg=%d:%u\n",
                         MDS_FW_VERSION, (unsigned)ESP.getPsramSize(), display_.hasFramebuffer() ? "psram" : "direct",
                         (unsigned long)millis(), (int)esp_reset_reason(), (unsigned long)linesReceived_,
                         (unsigned long)imagesReceived_, (unsigned long)discards_,
                         (unsigned long)transport_.droppedWrites(), (unsigned long)transport_.staleFlushes(), (int)lastImageResult_, (unsigned)lastImageBytes_);
  if (n > 0) transport_.write(line, static_cast<size_t>(n) < sizeof(line) ? n : sizeof(line) - 1);
}

void App::ack() {
  if (sendTtyAck_) transport_.write("ttyack;");
}
