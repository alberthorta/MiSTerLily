#include "app/App.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "Crc32.h"
#include "config.h"
#include "ui/CoreScreen.h"
#include "ui/LegacyCanvas.h"
#include "ui/SplashScreen.h"
#include "ui/Theme.h"
#include "util/Log.h"

namespace {

constexpr size_t kReadChunkBytes = 512;
// Upper bound per loop() call, so a host that never stops sending cannot starve the timers.
constexpr int kMaxReadsPerLoop = 32;
// A screenshot the host stops reading is abandoned after this long without progress.
constexpr uint32_t kShotStallMs = 1000;
// Lets the "ttyack;" of CMDRESET leave before the chip restarts.
constexpr uint32_t kRestartDelayMs = 100;
constexpr uint32_t kMsPerMinute = 60000;
constexpr const char* kWaitingText = "Waiting for MiSTer";

}  // namespace

App::App(DisplayManager& display, SerialTransport& transport, Settings& settings)
    : display_(display), transport_(transport), settings_(settings), parser_(*this) {}

void App::begin() {
  assetBuffer_ = static_cast<uint8_t*>(heap_caps_malloc(proto::kMaxAssetBytes, MALLOC_CAP_SPIRAM));
  parser_.setAssetBuffer(assetBuffer_, assetBuffer_ ? proto::kMaxAssetBytes : 0);
  LOGF("asset buffer %s", assetBuffer_ ? "ok" : "missing");
  presence_.setIdleTimeout(settings_.idleMinutes() * kMsPerMinute);
  presence_.start(millis());

  // The sketch prints "ttyrdy;" here, but no tty2oled script reads it. Over USB it stays
  // queued until the next reply and then corrupts that reply (installer.sh reads one line for
  // CMDHWINF), so we do not send it.
}

void App::loop() {
  uint8_t buffer[kReadChunkBytes];
  // Read until the FIFO is empty: while it is full the host is held off (NAK).
  for (int i = 0; i < kMaxReadsPerLoop; ++i) {
    const size_t n = transport_.read(buffer, sizeof(buffer));
    if (n == 0) break;
    presence_.onTraffic(millis());
    parser_.feed(buffer, n, millis());
  }
  parser_.poll(millis());
  handlePresence(presence_.update(millis()));
  pumpScreenshot(millis());
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
  if (changesScreen(cmd.id)) noteActivity();
  handle(cmd, text);
  ack();
}

void App::onImage(const proto::ImageFrame& frame) {
  ++imagesReceived_;
  lastImageResult_ = frame.result;
  lastImageBytes_ = frame.length;
  LOGF("image '%s' result %d format %d bytes %u", frame.commandLine, (int)frame.result, (int)frame.format,
       (unsigned)frame.length);

  noteActivity();
  const std::string_view command(frame.commandLine);
  const proto::CoreCommand cc = proto::parseCoreCommand(command);
  setCore(cc.core);
  assetValid_ = false;
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

void App::onAsset(const proto::AssetFrame& frame) {
  ++imagesReceived_;
  noteActivity();
  const std::string_view header(frame.commandLine);
  const char* error = nullptr;
  // The parser already wrote over the buffer, so the previous asset is gone either way.
  assetValid_ = false;
  pictureFormat_ = proto::ImageFormat::None;
  // The core name is the last field; take it first so a failed picture still shows the name.
  std::string_view f[4];
  if (header.size() > 7 && proto::splitFields(header.substr(7), f, 4) == 4) setCore(f[3]);
  if (frame.result == proto::AssetResult::Ok && decodeAsset(header, frame, &error)) {
    lastAssetError_ = "none";
    transport_.write("MDSIMG;ok\n");
  } else {
    if (frame.result == proto::AssetResult::BadHeader) error = "header";
    if (frame.result == proto::AssetResult::TooLarge) error = "size";
    if (frame.result == proto::AssetResult::Incomplete) error = "incomplete";
    lastAssetError_ = error ? error : "unknown";
    char reply[40];
    snprintf(reply, sizeof(reply), "MDSIMG;err=%s\n", lastAssetError_);
    transport_.write(reply);
    // Still a core change: show the name rather than the previous core's picture.
    showCore();
  }
  LOGF("asset '%s' result %d error %s", frame.commandLine, (int)frame.result, lastAssetError_);
  ack();
}

bool App::decodeAsset(std::string_view header, const proto::AssetFrame& frame, const char** error) {
  // MDSIMG,<bytes>,<crc32 hex>,<png|jpg>,<core>
  std::string_view f[4];
  if (proto::splitFields(header.substr(7), f, 4) != 4 || f[1].size() != 8) {
    *error = "header";
    return false;
  }
  uint32_t crc = 0;
  for (char c : f[1]) {
    int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
    if (v < 0) {
      *error = "header";
      return false;
    }
    crc = (crc << 4) | static_cast<uint32_t>(v);
  }
  if (proto::crc32(frame.data, frame.length) != crc) {
    *error = "crc";
    return false;
  }
  const bool png = f[2] == "png";
  if (!png && f[2] != "jpg") {
    *error = "format";
    return false;
  }
  if (!CoreScreen::drawAsset(display_, png ? CoreScreen::AssetFormat::Png : CoreScreen::AssetFormat::Jpeg,
                             frame.data, frame.length)) {
    *error = "decode";
    return false;
  }
  assetValid_ = true;
  assetIsPng_ = png;
  assetLength_ = frame.length;
  return true;
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
      assetValid_ = false;
      showCore();
      break;
    case CommandId::ShowCoreName:
      screen_ = Screen::Core;
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
    case CommandId::MdsPing: {
      // The uptime lets the daemon notice that the board restarted and resend the core.
      char reply[40];
      const int n = snprintf(reply, sizeof(reply), "MDSPONG;up=%lu\n", (unsigned long)millis());
      if (n > 0) transport_.write(reply, n);
      handlePresence(presence_.onHeartbeat(millis()));
      break;
    }
    case CommandId::MdsCfg:
      handleConfig(cmd.args);
      break;
    case CommandId::MdsHang:
#if MDS_TEST_COMMANDS
      LOGF("MDSHANG: spinning until the watchdog fires");
      for (;;) {
      }
#endif
      break;
    case CommandId::MdsInfo:
      sendInfo();
      break;
    case CommandId::MdsShot:
      startScreenshot();
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
  screen_ = Screen::Core;
  const std::string_view name(coreName_, coreNameLength_);
  if (assetValid_ && CoreScreen::drawAsset(display_, assetIsPng_ ? CoreScreen::AssetFormat::Png : CoreScreen::AssetFormat::Jpeg,
                                           assetBuffer_, assetLength_)) {
    return;
  }
  if (pictureFormat_ == proto::ImageFormat::None) {
    CoreScreen::drawNameOnly(display_, name);
    return;
  }
  const size_t length = pictureFormat_ == proto::ImageFormat::Xbm ? proto::kXbmBytes : proto::kGscBytes;
  CoreScreen::drawWithArtwork(display_, name, pictureFormat_, picture_, length);
}

void App::showSplash() {
  screen_ = Screen::Splash;
  SplashScreen::draw(display_, "v" MDS_FW_VERSION);
}

bool App::changesScreen(proto::CommandId id) {
  using proto::CommandId;
  switch (id) {
    case CommandId::CoreName:
    case CommandId::MdsCore:
    case CommandId::ShowCoreName:
    case CommandId::ShowPicture:
    case CommandId::ClearScreen:
    case CommandId::ClearWithTransition:
    case CommandId::DisplayUpdate:
    case CommandId::StartScreen:
    case CommandId::TestPicture:
    case CommandId::Text:
    case CommandId::Geometry:
    case CommandId::DisplayOn:
      return true;
    default:
      return false;  // pings, settings, queries and tty2oled startup noise do not wake the screen
  }
}

void App::noteActivity() {
  if (presence_.onActivity(millis()) == presence::Event::Wake) {
    LOGF("idle sleep: wake");
    display_.setBacklightOn(true);
  }
  if (screen_ == Screen::Waiting || screen_ == Screen::Splash) screen_ = Screen::Other;
}

void App::handlePresence(presence::Event event) {
  switch (event) {
    case presence::Event::HostLost:
      LOGF("host lost");
      screen_ = Screen::Waiting;
      SplashScreen::draw(display_, "v" MDS_FW_VERSION, kWaitingText);
      break;
    case presence::Event::SplashWaiting:
      if (screen_ == Screen::Splash) SplashScreen::draw(display_, "v" MDS_FW_VERSION, kWaitingText);
      break;
    case presence::Event::IdleSleep:
      LOGF("idle sleep");
      display_.setBacklightOn(false);
      break;
    case presence::Event::Wake:
      display_.setBacklightOn(true);
      break;
    case presence::Event::HostBack:  // the daemon resends the core when it reconnects
    case presence::Event::None:
      break;
  }
}

void App::handleConfig(std::string_view args) {
  // MDSCFG                -> list
  // MDSCFG,idle,<minutes> -> idle sleep after <minutes> without a core change (0 = never)
  // MDSCFG,reset          -> defaults
  std::string_view f[2];
  const size_t n = args.empty() ? 0 : proto::splitFields(args, f, 2);
  int32_t v = 0;
  if (n == 2 && f[0] == "idle" && proto::parseInt(f[1], v) && v >= 0 && v <= Settings::kMaxIdleMinutes) {
    settings_.setIdleMinutes(static_cast<uint16_t>(v));
    presence_.setIdleTimeout(settings_.idleMinutes() * kMsPerMinute);
  } else if (n == 1 && f[0] == "reset") {
    settings_.resetToDefaults();
    presence_.setIdleTimeout(0);
    display_.setBrightness(Settings::backlightFor(settings_.contrast()));
    display_.setRotation(cfg::kRotationNormal);
  } else if (n != 0) {
    transport_.write("MDSCFG;err=args\n");
    return;
  }
  sendConfig();
}

void App::sendConfig() {
  char line[80];
  const int n = snprintf(line, sizeof(line), "MDSCFG;contrast=%u;rotate=%d;idle=%u\n", settings_.contrast(),
                         settings_.rotated() ? 1 : 0, settings_.idleMinutes());
  if (n > 0) transport_.write(line, n);
}

void App::setContrast(std::string_view args) {
  int32_t contrast = 0;
  if (!proto::parseInt(args, contrast)) return;
  contrast = contrast < 0 ? 0 : (contrast > 255 ? 255 : contrast);
  settings_.setContrast(static_cast<uint8_t>(contrast));
  display_.setBrightness(Settings::backlightFor(settings_.contrast()));
}

void App::setRotation(std::string_view args) {
  int32_t r = 0;
  proto::parseInt(args, r);  // like toInt(): junk means 0
  settings_.setRotated(r == 1);
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
  char line[320];
  const int n = snprintf(line, sizeof(line),
                         "MDSINFO;fw=%s;board=T-Display-S3;psram=%u;fb=%s;uptime_ms=%lu;reset=%d;"
                         "lines=%lu;images=%lu;discards=%lu;txdrop=%lu;txstale=%lu;lastimg=%d:%u;asseterr=%s;"
                         "host=%s;idle=%s;core=%s\n",
                         MDS_FW_VERSION, (unsigned)ESP.getPsramSize(), display_.hasFramebuffer() ? "psram" : "direct",
                         (unsigned long)millis(), (int)esp_reset_reason(), (unsigned long)linesReceived_,
                         (unsigned long)imagesReceived_, (unsigned long)discards_,
                         (unsigned long)transport_.droppedWrites(), (unsigned long)transport_.staleFlushes(),
                         (int)lastImageResult_, (unsigned)lastImageBytes_, lastAssetError_,
                         presence_.heartbeatSeen() ? (presence_.hostLost() ? "lost" : "ok") : "none",
                         presence_.idleSleeping() ? "sleep" : "awake", coreName_);
  if (n > 0) transport_.write(line, static_cast<size_t>(n) < sizeof(line) ? n : sizeof(line) - 1);
}

void App::startScreenshot() {
  if (shotLeft_ > 0 || !display_.hasFramebuffer()) {
    transport_.write("MDSSHOT,0,0,0\n");
    return;
  }
  char header[48];
  const int n = snprintf(header, sizeof(header), "MDSSHOT,%d,%d,%u\n", display_.width(), display_.height(),
                         (unsigned)display_.framebufferSize());
  if (n <= 0 || !transport_.write(header, n)) return;
  shotData_ = display_.framebufferBytes();
  shotLeft_ = display_.framebufferSize();
  shotProgressMs_ = millis();
}

void App::pumpScreenshot(uint32_t nowMs) {
  if (shotLeft_ == 0) return;
  // Leave room so acks and replies are never refused while a screenshot is going out.
  constexpr size_t kReserve = 128;
  const size_t room = transport_.writeRoom();
  if (room > kReserve) {
    size_t n = room - kReserve;
    if (n > shotLeft_) n = shotLeft_;
    transport_.write(reinterpret_cast<const char*>(shotData_), n);
    shotData_ += n;
    shotLeft_ -= n;
    shotProgressMs_ = nowMs;
  } else if (nowMs - shotProgressMs_ >= kShotStallMs) {
    shotLeft_ = 0;
  }
}

void App::ack() {
  if (sendTtyAck_) transport_.write("ttyack;");
}
