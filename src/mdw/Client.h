// MyDesktopWidget Screens - the protocol core.
// SPDX-License-Identifier: MIT
//
// Everything the library does, in plain C++ with no Arduino dependency, so it can be tested on a PC.
// The Arduino class in MyDesktopWidget.h is a thin wrapper that supplies a serial port and the clock.
//
// The protocol is MyDesktopWidget's device protocol 1: a two-frame handshake (attach / ready), then
// the same frames MyDesktopWidget sends to its phone app - subscribe, subscribed, telemetry, paused.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include <ArduinoJson.h>

#ifndef MDW_MAX_FRAME
// The largest frame this device stores. Bigger frames (up to the protocol's 64 KiB) are read past
// without being stored, so the stream stays in step. A subscription to a few dozen readings fits
// easily in the default.
#define MDW_MAX_FRAME 8192
#endif

#ifndef MDW_MAX_SUBSCRIPTIONS
#define MDW_MAX_SUBSCRIPTIONS 16
#endif

#ifndef MDW_MAX_TILE_PIXELS
// The largest tile this device decodes, in pixels: 40 x 40, which is what MyDesktopWidget sends a board
// whose frame ceiling is 4 KB or more. A bigger tile is acknowledged and dropped.
#define MDW_MAX_TILE_PIXELS 1600
#endif

#ifndef MDW_MAX_ID_LENGTH
// The protocol's own limit on a sensor id.
#define MDW_MAX_ID_LENGTH 64
#endif

namespace mdwlib {

/// The protocol version this library speaks.
constexpr int kProtocol = 1;

/// The largest frame the protocol allows. A header claiming more is not a header.
constexpr uint32_t kProtocolMaxFrame = 65536;

/// The bytes a device reads and writes. Non-blocking: read returns what has already arrived.
class Port {
 public:
  virtual ~Port() = default;
  virtual size_t read(uint8_t* buffer, size_t capacity) = 0;
  virtual void write(const uint8_t* data, size_t length) = 0;
};

using ValueCallback = std::function<void(float value)>;
using FamilyCallback = std::function<void(const char* id, float value)>;
using TextCallback = std::function<void(const char* text)>;
using MissingCallback = std::function<void()>;
using ConnectionCallback = std::function<void(bool connected, const char* machine)>;
using PausedCallback = std::function<void(const char* reason, const char* message)>;

/// One tile of the picture MyDesktopWidget draws for this screen: `w x h` RGB565 pixels, row by row,
/// to be drawn with its top-left corner at (x, y). The pixels are valid only during the call.
using TileCallback = std::function<void(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* pixels)>;

/// Counters, for a sketch that wants to show what the link is doing.
struct Stats {
  uint32_t frames = 0;        // whole frames received
  uint32_t skippedBytes = 0;  // bytes that were not part of a frame (an ESP32 boot banner is normal)
  uint32_t oversized = 0;     // frames larger than MDW_MAX_FRAME, read past and not stored
  uint32_t stalls = 0;        // frames that stopped half way
  uint32_t malformed = 0;     // frames that were not valid JSON
  uint32_t tiles = 0;         // image tiles drawn
  uint32_t tilesDropped = 0;  // image tiles that could not be drawn (and were still acknowledged)
};

class Client {
 public:
  static constexpr uint32_t kAnnounceMs = 1000;         // attach, once a second until answered
  static constexpr uint32_t kRefusedAnnounceMs = 10000; // ...and every ten seconds after a refusal
  static constexpr uint32_t kHostGoneMs = 5000;         // no frame for this long: the engine has gone
  static constexpr uint32_t kQuietGapMs = 50;           // a quiet line is a frame boundary
  static constexpr uint32_t kStallMs = 250;             // a frame stopped half way this long is dropped

  explicit Client(Port& port);

  /// Who this device is. `id` is the ESP32's MAC as 12 uppercase hex digits; the rest may be null.
  void setIdentity(const char* id, const char* name, const char* board, const char* firmware);

  /// Calls back with the reading on every frame that carries it.
  bool subscribe(const char* id, ValueCallback callback);

  /// `prefix*` - a family: calls back once per matching reading on every frame.
  bool subscribe(const char* prefix, FamilyCallback callback);

  /// Calls back with a reading whose value is words (a media title, a drive letter).
  bool onText(const char* id, TextCallback callback);

  /// Calls back when a reading has no value. Absent is not zero: show "--", not 0.
  bool onMissing(const char* id, MissingCallback callback);

  void onConnection(ConnectionCallback callback) { connection_ = std::move(callback); }
  void onPaused(PausedCallback callback) { paused_ = std::move(callback); }

  /// Asks for image mode: MyDesktopWidget draws one of its skins at this size and sends it as tiles.
  /// The size is the panel's in the orientation the sketch draws in. Call before the first loop().
  void setScreen(uint16_t width, uint16_t height);

  /// Called with each tile to draw. Without one, tiles are acknowledged and dropped.
  void onTile(TileCallback callback) { tile_ = std::move(callback); }

  /// Whether MyDesktopWidget granted image mode on this connection.
  bool showsImages() const { return attached_ && imageGranted_; }

  /// Does everything. Call it from loop() with the time in milliseconds.
  void loop(uint32_t nowMs);

  bool connected() const { return attached_; }
  const char* machine() const { return machine_; }

  /// Why the engine last refused this device, or "" if it has not.
  const char* refusal() const { return refusal_; }

  const Stats& stats() const { return stats_; }

 private:
  struct Subscription {
    char id[MDW_MAX_ID_LENGTH + 1] = {0};
    bool family = false;
    bool known = false;  // a frame has said whether it is present
    bool present = false;
    ValueCallback value;
    FamilyCallback familyValue;
    TextCallback text;
    MissingCallback missing;
  };

  Subscription* find(const char* id);
  Subscription* findOrAdd(const char* id, bool family);
  void readAvailable(uint32_t now);
  void consume(uint8_t byte, uint32_t now);
  void resetFrame();
  void handleFrame(uint32_t length, uint32_t now);
  void handleJson(const char* json, size_t length, uint32_t now);
  void handleTile(const uint8_t* frame, size_t length);
  void sendDrawn(uint16_t sequence);
  void dispatchTelemetry(JsonDocument& document);
  void announce(uint32_t now);
  void sendSubscribe();
  void sendJson(const JsonDocument& document);
  void lose();

  Port& port_;
  char id_[13] = {0};
  char name_[65] = {0};
  char board_[65] = {0};
  char firmware_[65] = {0};

  Subscription subscriptions_[MDW_MAX_SUBSCRIPTIONS];
  size_t subscriptionCount_ = 0;

  ConnectionCallback connection_;
  PausedCallback paused_;

  // The reader.
  uint8_t frame_[MDW_MAX_FRAME];
  uint8_t header_[4] = {0};
  uint8_t headerGot_ = 0;
  uint32_t need_ = 0;
  uint32_t got_ = 0;
  bool inFrame_ = false;
  bool draining_ = false;
  uint32_t lastByteMs_ = 0;

  // The link.
  bool started_ = false;
  bool attached_ = false;
  uint32_t lastAnnounceMs_ = 0;
  uint32_t lastFromHostMs_ = 0;
  uint32_t announceEveryMs_ = kAnnounceMs;
  char machine_[65] = {0};
  char refusal_[33] = {0};

  // Image mode.
  uint16_t screenWidth_ = 0;
  uint16_t screenHeight_ = 0;
  bool imageGranted_ = false;
  TileCallback tile_;
  uint16_t pixels_[MDW_MAX_TILE_PIXELS];

  Stats stats_;
};

}  // namespace mdwlib
