// MyDesktopWidget Screens - live PC stats from MyDesktopWidget on an ESP32 screen.
// https://github.com/mydesktopwidget/mydesktopwidget-screens
// SPDX-License-Identifier: MIT
//
//   #include <MyDesktopWidget.h>
//
//   MyDesktopWidget mdw(Serial);
//
//   void setup() {
//     mdw.begin("Desk gauge");
//     mdw.subscribe("cpu.load.cpu_total", [](float load) { /* draw it */ });
//     mdw.onMissing("cpu.load.cpu_total", [] { /* draw "--": absent is not zero */ });
//   }
//
//   void loop() { mdw.loop(); }
//
// The USB serial port belongs to the library once begin() is called: do not print to Serial from
// your sketch, or the PC will read your text as garbage between frames.

#pragma once

#include <Arduino.h>

#include "mdw/Client.h"

#define MYDESKTOPWIDGET_VERSION "0.1.0"

class MyDesktopWidget {
 public:
  /// The usual case: the board's own USB serial port. begin() starts it at the right speed.
  explicit MyDesktopWidget(HardwareSerial& serial);

  /// Any other stream (an ESP32-S3's native USB, for instance). Start it yourself before begin().
  explicit MyDesktopWidget(Stream& stream);

  /// Names this screen in MyDesktopWidget's Settings, and starts talking to the PC.
  void begin(const char* name, unsigned long baud = 921600);

  /// The board-profile id, such as "cyd-2432s028r". Optional; call before begin().
  void setBoard(const char* board) { board_ = board; }

  bool subscribe(const char* id, mdwlib::ValueCallback callback) { return client_.subscribe(id, std::move(callback)); }
  bool subscribe(const char* prefix, mdwlib::FamilyCallback callback) { return client_.subscribe(prefix, std::move(callback)); }
  bool onText(const char* id, mdwlib::TextCallback callback) { return client_.onText(id, std::move(callback)); }
  bool onMissing(const char* id, mdwlib::MissingCallback callback) { return client_.onMissing(id, std::move(callback)); }
  void onConnection(mdwlib::ConnectionCallback callback) { client_.onConnection(std::move(callback)); }
  void onPaused(mdwlib::PausedCallback callback) { client_.onPaused(std::move(callback)); }

  /// Call from loop(), as often as you can. It never blocks.
  void loop() { client_.loop(millis()); }

  /// Whether MyDesktopWidget is running and sending readings.
  bool connected() const { return client_.connected(); }

  /// The PC's name, once connected.
  const char* machine() const { return client_.machine(); }

  /// Why MyDesktopWidget refused this screen, or "" - "protocol.version" means update the library.
  const char* refusal() const { return client_.refusal(); }

  const mdwlib::Stats& stats() const { return client_.stats(); }

 private:
  // Reads the board's serial port in blocks when it can: HardwareSerial has a bulk read, and Stream's
  // readBytes is not virtual, so through a Stream& it falls back to one locked read per byte.
  class ArduinoPort : public mdwlib::Port {
   public:
    ArduinoPort(HardwareSerial* serial, Stream& stream) : serial_(serial), stream_(stream) {}
    size_t read(uint8_t* buffer, size_t capacity) override;
    void write(const uint8_t* data, size_t length) override { stream_.write(data, length); }

   private:
    HardwareSerial* serial_;
    Stream& stream_;
  };

  HardwareSerial* serial_ = nullptr;
  ArduinoPort port_;
  mdwlib::Client client_;
  const char* board_ = nullptr;
};
