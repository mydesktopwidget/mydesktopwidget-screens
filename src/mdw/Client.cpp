// MyDesktopWidget Screens - the protocol core.
// SPDX-License-Identifier: MIT

#include "Client.h"

#include <cstring>

namespace mdwlib {

namespace {

void copy(char* to, size_t size, const char* from) {
  if (size == 0) return;
  size_t i = 0;
  if (from != nullptr) {
    for (; i + 1 < size && from[i] != '\0'; ++i) to[i] = from[i];
  }
  to[i] = '\0';
}

// A family is written `gpu.temperature.*`; what is matched is everything before the star.
size_t familyPrefixLength(const char* id) { return std::strlen(id) - 1; }

bool isFamily(const char* id) {
  const size_t length = std::strlen(id);
  return length >= 2 && id[length - 1] == '*';
}

}  // namespace

Client::Client(Port& port) : port_(port) {}

void Client::setIdentity(const char* id, const char* name, const char* board, const char* firmware) {
  copy(id_, sizeof(id_), id);
  copy(name_, sizeof(name_), name);
  copy(board_, sizeof(board_), board);
  copy(firmware_, sizeof(firmware_), firmware);
}

Client::Subscription* Client::find(const char* id) {
  for (size_t i = 0; i < subscriptionCount_; ++i) {
    if (std::strcmp(subscriptions_[i].id, id) == 0) return &subscriptions_[i];
  }
  return nullptr;
}

const Client::Subscription* Client::find(const char* id) const {
  for (size_t i = 0; i < subscriptionCount_; ++i) {
    if (std::strcmp(subscriptions_[i].id, id) == 0) return &subscriptions_[i];
  }
  return nullptr;
}

Client::Subscription* Client::findOrAdd(const char* id, bool family) {
  if (id == nullptr || id[0] == '\0' || std::strlen(id) > MDW_MAX_ID_LENGTH) return nullptr;
  if (isFamily(id) != family) return nullptr;

  if (Subscription* existing = find(id)) return existing;
  if (subscriptionCount_ >= MDW_MAX_SUBSCRIPTIONS) return nullptr;

  Subscription& added = subscriptions_[subscriptionCount_++];
  copy(added.id, sizeof(added.id), id);
  added.family = family;

  // The engine keeps no memory of what a device asked for, so a change while connected is sent now.
  if (attached_) sendSubscribe();

  return &added;
}

bool Client::subscribe(const char* id, ValueCallback callback) {
  Subscription* s = findOrAdd(id, false);
  if (s == nullptr) return false;
  s->value = std::move(callback);
  return true;
}

bool Client::subscribe(const char* prefix, FamilyCallback callback) {
  Subscription* s = findOrAdd(prefix, true);
  if (s == nullptr) return false;
  s->familyValue = std::move(callback);
  return true;
}

bool Client::onText(const char* id, TextCallback callback) {
  Subscription* s = findOrAdd(id, false);
  if (s == nullptr) return false;
  s->text = std::move(callback);
  return true;
}

bool Client::onMissing(const char* id, MissingCallback callback) {
  Subscription* s = findOrAdd(id, false);
  if (s == nullptr) return false;
  s->missing = std::move(callback);
  return true;
}

bool Client::watch(const char* id) { return findOrAdd(id, false) != nullptr; }

// The one place absent becomes NAN: `present` is false before the first frame, while the PC does not
// send the reading, and after MyDesktopWidget goes away - so nothing else needs to clear the value.
float Client::last(const char* id) const {
  const Subscription* s = id == nullptr ? nullptr : find(id);
  return s != nullptr && !s->family && s->present ? s->lastValue : NAN;
}

const char* Client::lastText(const char* id) const {
  const Subscription* s = id == nullptr ? nullptr : find(id);
  return s != nullptr && !s->family && s->present ? s->lastText : "";
}

void Client::setScreen(uint16_t width, uint16_t height) {
  screenWidth_ = width;
  screenHeight_ = height;
}

void Client::loop(uint32_t now) {
  if (!started_) {
    started_ = true;
    lastByteMs_ = now;
    announce(now);
  }

  readAvailable(now);

  // A frame that stopped half way will not finish: the sender died or bytes were lost.
  if ((headerGot_ > 0 || inFrame_) && now - lastByteMs_ > kStallMs) {
    ++stats_.stalls;
    resetFrame();
  }

  if (draining_ && now - lastByteMs_ > kQuietGapMs) draining_ = false;

  // Over USB nothing tells a device the engine went away - a closed program leaves a silent line -
  // and the engine is never quiet while it is there, so silence is the signal.
  if (attached_ && now - lastFromHostMs_ > kHostGoneMs) lose();

  if (!attached_ && now - lastAnnounceMs_ >= announceEveryMs_) announce(now);
}

void Client::readAvailable(uint32_t now) {
  // Read in blocks, never a byte at a time: on an ESP32 a per-byte read takes the UART driver's lock
  // each time, managed about 15 KB/s, and the driver flushed its buffer when it overflowed.
  uint8_t chunk[256];
  size_t budget = 4096;  // per loop(), so a flood cannot starve the sketch

  while (budget > 0) {
    const size_t n = port_.read(chunk, sizeof(chunk) < budget ? sizeof(chunk) : budget);
    if (n == 0) return;
    budget -= n;
    lastByteMs_ = now;
    for (size_t i = 0; i < n; ++i) consume(chunk[i], now);
  }
}

void Client::resetFrame() {
  headerGot_ = 0;
  need_ = 0;
  got_ = 0;
  inFrame_ = false;
}

void Client::consume(uint8_t byte, uint32_t now) {
  if (draining_) {
    // After a header nobody could believe, the only safe boundary is a quiet line: the engine writes
    // each frame as one burst.
    ++stats_.skippedBytes;
    return;
  }

  if (!inFrame_) {
    header_[headerGot_++] = byte;
    if (headerGot_ < 4) return;

    const uint32_t length = (uint32_t(header_[0]) << 24) | (uint32_t(header_[1]) << 16) |
                            (uint32_t(header_[2]) << 8) | uint32_t(header_[3]);
    headerGot_ = 0;

    // Bounded before anything is stored: a length is checked before a byte of the frame is kept.
    if (length == 0 || length > kProtocolMaxFrame) {
      stats_.skippedBytes += 4;
      draining_ = true;
      return;
    }

    need_ = length;
    got_ = 0;
    inFrame_ = true;
    return;
  }

  if (got_ < MDW_MAX_FRAME) frame_[got_] = byte;
  ++got_;

  if (got_ == need_) {
    const uint32_t length = need_;
    resetFrame();
    handleFrame(length, now);
  }
}

void Client::handleFrame(uint32_t length, uint32_t now) {
  ++stats_.frames;

  // Any frame at all is the engine being there, including one too big to keep.
  lastFromHostMs_ = now;

  if (length > MDW_MAX_FRAME) {
    ++stats_.oversized;
    return;
  }

  // The first byte says what the frame is: `{` is JSON, 0x01 an image tile - which a values-only
  // device is never sent, and a device that did not ask for images ignores.
  if (frame_[0] == 0x01) {
    if (imageGranted_) handleTile(frame_, length);
    return;
  }

  if (frame_[0] != '{') return;

  handleJson(reinterpret_cast<const char*>(frame_), length, now);
}

void Client::handleJson(const char* json, size_t length, uint32_t now) {
  JsonDocument document;

  if (deserializeJson(document, json, length) != DeserializationError::Ok) {
    ++stats_.malformed;
    return;
  }

  const char* type = document["type"] | "";

  if (std::strcmp(type, "ready") == 0) {
    attached_ = true;
    announceEveryMs_ = kAnnounceMs;
    refusal_[0] = '\0';
    copy(machine_, sizeof(machine_), document["machine"] | "");
    lastFromHostMs_ = now;

    imageGranted_ = false;
    for (JsonVariant mode : document["modes"].as<JsonArray>()) {
      if (std::strcmp(mode | "", "image") == 0) imageGranted_ = screenWidth_ > 0;
    }

    // Re-sent on every ready: the engine never remembers a subscription.
    sendSubscribe();

    if (connection_) connection_(true, machine_);
  } else if (std::strcmp(type, "telemetry") == 0) {
    if (attached_) dispatchTelemetry(document);
  } else if (std::strcmp(type, "paused") == 0) {
    if (paused_) paused_(document["reason"] | "", document["message"] | "");
  } else if (std::strcmp(type, "refused") == 0) {
    copy(refusal_, sizeof(refusal_), document["reason"] | "refused");

    // Still asked again, in case the engine is updated or switched on - but slowly, because a
    // refusal is an answer and asking every second would fill its log.
    announceEveryMs_ = kRefusedAnnounceMs;
    lastAnnounceMs_ = now;

    if (attached_) lose();
  }
  // Anything else - subscribed, or a frame a newer engine sends - is not an error.
}

void Client::dispatchTelemetry(JsonDocument& document) {
  JsonObject readings = document["readings"];
  JsonObject text = document["text"];

  for (size_t i = 0; i < subscriptionCount_; ++i) {
    Subscription& s = subscriptions_[i];

    if (s.family) {
      if (!s.familyValue || readings.isNull()) continue;
      const size_t prefix = familyPrefixLength(s.id);
      for (JsonPair pair : readings) {
        const char* key = pair.key().c_str();
        if (std::strncmp(key, s.id, prefix) == 0 && pair.value().is<float>()) {
          s.familyValue(key, pair.value().as<float>());
        }
      }
      continue;
    }

    JsonVariant value = readings[s.id];
    JsonVariant words = text[s.id];

    if (value.is<float>()) {
      s.known = true;
      s.present = true;
      s.lastValue = value.as<float>();
      s.lastText[0] = '\0';
      if (s.value) s.value(s.lastValue);
    } else if (words.is<const char*>()) {
      s.known = true;
      s.present = true;
      s.lastValue = NAN;
      copy(s.lastText, sizeof(s.lastText), words.as<const char*>());
      if (s.text) s.text(words.as<const char*>());
    } else {
      // Told once, when it goes missing, rather than on every frame - a sketch redrawing "--" twice a
      // second for something that is simply not there is wasted work on a small screen.
      if (!s.known || s.present) {
        s.known = true;
        s.present = false;
        if (s.missing) s.missing();
      }
    }
  }
}

void Client::handleTile(const uint8_t* frame, size_t length) {
  // Every tile is acknowledged, drawn or dropped (section 5.3): the engine keeps two in flight and
  // presumes a silent one lost, which costs the whole screen being sent again.
  if (length < 12) {
    ++stats_.tilesDropped;
    return;
  }

  auto u16 = [frame](size_t at) { return uint16_t((uint16_t(frame[at]) << 8) | frame[at + 1]); };

  const uint16_t sequence = u16(1);
  const uint16_t x = u16(3), y = u16(5), w = u16(7), h = u16(9);
  const uint8_t encoding = frame[11];
  const uint8_t* bytes = frame + 12;
  const size_t count = size_t(w) * h;
  const size_t available = length - 12;

  bool ok = w > 0 && h > 0 && uint32_t(x) + w <= screenWidth_ && uint32_t(y) + h <= screenHeight_ &&
            count <= MDW_MAX_TILE_PIXELS;

  if (ok && encoding == 2) {
    ok = available == count * 2;
    for (size_t i = 0; ok && i < count; ++i) pixels_[i] = u16(12 + i * 2);
  } else if (ok && encoding == 3) {
    size_t produced = 0;
    ok = available % 3 == 0;
    for (size_t at = 0; ok && at < available; at += 3) {
      const uint8_t run = bytes[at];
      const uint16_t pixel = uint16_t((uint16_t(bytes[at + 1]) << 8) | bytes[at + 2]);
      if (run == 0 || produced + run > count) {
        ok = false;
        break;
      }
      for (uint8_t i = 0; i < run; ++i) pixels_[produced++] = pixel;
    }
    ok = ok && produced == count;
  } else {
    ok = false;
  }

  if (ok && tile_) {
    tile_(x, y, w, h, pixels_);
    ++stats_.tiles;
  } else {
    ++stats_.tilesDropped;
  }

  sendDrawn(sequence);
}

void Client::sendDrawn(uint16_t sequence) {
  JsonDocument document;
  document["v"] = 1;
  document["type"] = "drawn";
  document["tile"] = sequence;
  sendJson(document);
}

void Client::lose() {
  attached_ = false;
  imageGranted_ = false;

  for (size_t i = 0; i < subscriptionCount_; ++i) {
    Subscription& s = subscriptions_[i];
    const bool wasPresent = s.present;
    s.known = false;
    s.present = false;
    if (wasPresent && s.missing) s.missing();
  }

  if (connection_) connection_(false, machine_);
}

void Client::announce(uint32_t now) {
  lastAnnounceMs_ = now;

  JsonDocument document;
  document["v"] = 1;
  document["type"] = "attach";
  document["protocol"] = kProtocol;
  document["id"] = id_;
  document["device"] = name_;
  if (board_[0] != '\0') document["board"] = board_;
  if (firmware_[0] != '\0') document["firmware"] = firmware_;
  document["modes"].add("values");

  if (screenWidth_ > 0 && screenHeight_ > 0) {
    document["modes"].add("image");
    document["screen"]["width"] = screenWidth_;
    document["screen"]["height"] = screenHeight_;
    document["maxFrame"] = MDW_MAX_FRAME;
  }

  sendJson(document);
}

void Client::sendSubscribe() {
  JsonDocument document;
  document["v"] = 1;
  document["type"] = "subscribe";
  JsonArray ids = document["ids"].to<JsonArray>();
  for (size_t i = 0; i < subscriptionCount_; ++i) ids.add(subscriptions_[i].id);

  sendJson(document);
}

void Client::sendJson(const JsonDocument& document) {
  // Header and payload in one write, so nothing between the two can split a frame across a quiet gap.
  static uint8_t buffer[4 + 64 + MDW_MAX_SUBSCRIPTIONS * (MDW_MAX_ID_LENGTH + 3)];

  const size_t length = serializeJson(document, reinterpret_cast<char*>(buffer + 4), sizeof(buffer) - 4);
  if (length == 0 || length >= sizeof(buffer) - 4) return;

  buffer[0] = uint8_t(length >> 24);
  buffer[1] = uint8_t(length >> 16);
  buffer[2] = uint8_t(length >> 8);
  buffer[3] = uint8_t(length);

  port_.write(buffer, length + 4);
}

}  // namespace mdwlib
