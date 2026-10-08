#include "app/App.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "ui/CoreScreen.h"
#include "ui/LegacyCanvas.h"
#include "ui/SplashScreen.h"
#include "ui/Theme.h"
#include "util/Log.h"

namespace {

constexpr size_t kReadChunkBytes = 512;
// Lets the "ttyack;" of CMDRESET leave before the chip restarts.
constexpr uint32_t kRestartDelayMs = 100;

// CMDCON is OLED contrast 0..255 (tty2oled.sh sends CONTRAST=100 by default). On a backlit TFT
// 0 must not mean "black", so map 0..255 onto a usable backlight range.
constexpr uint8_t kMinBacklight = 40;
uint8_t backlightForContrast(int32_t contrast) {
  if (contrast < 0) contrast = 0;
  if (contrast > 255) contrast = 255;
  return static_cast<uint8_t>(kMinBacklight + contrast * (255 - kMinBacklight) / 255);
}

}  // namespace

App::App(DisplayManager& display, SerialTransport& transport)
    : display_(display), transport_(transport), parser_(*this) {}

void App::begin() {
  // The sketch prints "ttyrdy;" here, but no tty2oled script reads it. Over USB it stays
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
  const std::string_view text(line, length);
  const proto::Command cmd = proto::parseCommand(text);
  LOGF("line '%s' -> %d", line, (int)cmd.id);
  handle(cmd, text);
  ack();
}

void App::onImage(const proto::ImageFrame& frame) {
  ++imagesReceived_;
  lastImageResult_ = frame.result;
  lastImageBytes_ = frame.length;
  LOGF("image '%s' result %d format %d bytes %u", frame.commandLine, (int)frame.result, (int)frame.format,
       (unsigned)frame.length);

  const std::string_view command(frame.commandLine);
  const proto::CoreCommand cc = proto::parseCoreCommand(command);
  setCore(cc.core);
  if (frame.result == proto::ImageResult::Ok) {
    memcpy(picture_, frame.data, frame.length);
    pictureFormat_ = frame.format;
  } else {
    // A missing or cut picture: show the name rather than an error or stale artwork.
    pictureFormat_ = proto::ImageFormat::None;
  }
  // CMDAPD only loads the picture; CMDSPIC shows it later.
  if (command.substr(0, 6) == "CMDCOR") showCore();
  ack();
}

void App::onDiscard(proto::DiscardReason reason, size_t length) {
  ++discards_;
  LOGF("discard reason %d, %u bytes", (int)reason, (unsigned)length);
}

void App::handle(const proto::Command& cmd, std::string_view line) {
  using proto::CommandId;
  bool present = true;
  switch (cmd.id) {
    case CommandId::CoreName:  // tty2oled sends the bare name when it has no picture
    case CommandId::MdsCore:
      setCore(cmd.args);
      pictureFormat_ = proto::ImageFormat::None;
      showCore();
      break;
    case CommandId::ShowCoreName:
      CoreScreen::drawNameOnly(display_, std::string_view(coreName_, coreNameLength_));
      break;
    case CommandId::ShowPicture:
      showCore();
      break;

    case CommandId::ClearScreen:
      display_.canvas().fillScreen(theme::kBackground);
      display_.present();
      break;
    case CommandId::ClearBuffer:
      display_.canvas().fillScreen(theme::kBackground);
      break;
    case CommandId::ClearWithTransition:
      clearWithFill(cmd.args);
      break;
    case CommandId::DisplayUpdate:
      display_.present();
      break;
    case CommandId::StartScreen:
      showSplash();
      break;
    case CommandId::TestPicture:
      LegacyCanvas::testPattern(display_);
      break;
    case CommandId::Text:
      if (LegacyCanvas::text(display_, cmd.args, &present)) {
        if (present) display_.present();
      } else {
        LegacyCanvas::parameterError(display_, line);
      }
      break;
    case CommandId::Geometry:
      if (LegacyCanvas::geometry(display_, cmd.args, &present)) {
        if (present) display_.present();
      } else {
        LegacyCanvas::parameterError(display_, line);
      }
      break;

    case CommandId::DisplayOff:
      display_.sleep();
      break;
    case CommandId::DisplayOn:
      display_.wake();
      break;
    case CommandId::Contrast:
      setContrast(cmd.args);
      break;
    case CommandId::Rotate:
      setRotation(cmd.args);
      break;

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
      // Accepted and ignored: QWERTZ, CMDNULL, CMDSETTIME, CMDSAVER, CMDSWSAVER, CMDSECD, unknown CMD*.
      break;
  }
}

void App::setCore(std::string_view name) {
  coreNameLength_ = name.size() < kMaxCoreName ? name.size() : kMaxCoreName;
  memcpy(coreName_, name.data(), coreNameLength_);
  coreName_[coreNameLength_] = '\0';
}

void App::showCore() {
  // Phase 4 draws the picture when pictureFormat_ != None.
  CoreScreen::drawNameOnly(display_, std::string_view(coreName_, coreNameLength_));
}

void App::showSplash() { SplashScreen::draw(display_, "v" MDS_FW_VERSION); }

void App::setContrast(std::string_view args) {
  int32_t contrast = 0;
  if (!proto::parseInt(args, contrast)) return;
  display_.setBrightness(backlightForContrast(contrast));
}

void App::setRotation(std::string_view args) {
  int32_t r = 0;
  proto::parseInt(args, r);  // like toInt(): junk means 0
  display_.setRotation(r == 1 ? cfg::kRotationFlipped : cfg::kRotationNormal);
}

void App::clearWithFill(std::string_view args) {
  // CMDCLST,t,c: fill with grey level c. The transition t is ignored for now.
  std::string_view f[2];
  int32_t level = 0;
  if (proto::splitFields(args, f, 2) == 2) proto::parseInt(f[1], level);
  pictureFormat_ = proto::ImageFormat::None;  // the sketch clears its picture buffer too
  display_.canvas().fillScreen(LegacyCanvas::grey(level));
  display_.present();
}

void App::sendHardwareInfo() {
  // installer.sh reads this with `read`, takes ${BLA:0:9} as the board id and ${SWver:10} as
  // the version. The id is exactly 9 characters and unknown to the tty2oled flasher; the version
  // starts with a letter, so it compares newer than any tty2oled build date and no update is offered.
  transport_.write("HWTDISPS3;MDS-" MDS_FW_VERSION ";\n");
}

void App::sendInfo() {
  char line[240];
  const int n = snprintf(line, sizeof(line),
                         "MDSINFO;fw=%s;board=T-Display-S3;psram=%u;fb=%s;uptime_ms=%lu;reset=%d;"
                         "lines=%lu;images=%lu;discards=%lu;txdrop=%lu;txstale=%lu;lastimg=%d:%u;core=%s\n",
                         MDS_FW_VERSION, (unsigned)ESP.getPsramSize(), display_.hasFramebuffer() ? "psram" : "direct",
                         (unsigned long)millis(), (int)esp_reset_reason(), (unsigned long)linesReceived_,
                         (unsigned long)imagesReceived_, (unsigned long)discards_,
                         (unsigned long)transport_.droppedWrites(), (unsigned long)transport_.staleFlushes(),
                         (int)lastImageResult_, (unsigned)lastImageBytes_, coreName_);
  if (n > 0) transport_.write(line, static_cast<size_t>(n) < sizeof(line) ? n : sizeof(line) - 1);
}

void App::ack() {
  if (sendTtyAck_) transport_.write("ttyack;");
}
