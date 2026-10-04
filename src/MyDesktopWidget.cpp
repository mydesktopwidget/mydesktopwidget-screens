// MyDesktopWidget Screens - the Arduino wrapper around mdwlib::Client.
// SPDX-License-Identifier: MIT

#include "MyDesktopWidget.h"

#include <esp_system.h>

MyDesktopWidget::MyDesktopWidget(HardwareSerial& serial)
    : serial_(&serial), port_(&serial, serial), client_(port_) {}

MyDesktopWidget::MyDesktopWidget(Stream& stream) : port_(nullptr, stream), client_(port_) {}

void MyDesktopWidget::begin(const char* name, unsigned long baud) {
  if (serial_ != nullptr) {
    // Before begin(): the receive buffer is sized when the port starts. The default 256 bytes is a
    // third of a telemetry frame at 921,600 baud, and the core flushes a buffer that overflows.
    serial_->setRxBufferSize(4096);
    serial_->begin(baud);
  }

  // The id MyDesktopWidget keys this screen's settings by: the ESP32's base MAC, which survives a
  // reflash and a change of USB port. The COM port cannot be the id - Windows renumbers ports, and a
  // CH340 has no serial number, so two identical boards look the same from the PC.
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);

  char id[13];
  snprintf(id, sizeof(id), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  client_.setIdentity(id, name, board_, "MyDesktopWidget library " MYDESKTOPWIDGET_VERSION);
}

size_t MyDesktopWidget::ArduinoPort::read(uint8_t* buffer, size_t capacity) {
  if (serial_ != nullptr) {
    const size_t waiting = serial_->available();
    if (waiting == 0) return 0;
    return serial_->read(buffer, waiting < capacity ? waiting : capacity);
  }

  size_t n = 0;
  while (n < capacity && stream_.available() > 0) {
    const int c = stream_.read();
    if (c < 0) break;
    buffer[n++] = uint8_t(c);
  }
  return n;
}
