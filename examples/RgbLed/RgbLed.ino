// RgbLed - the smallest sketch there is: an LED that goes from green to red with the PC's CPU load.
//
// Written for the RGB LED on the back of a Cheap Yellow Display (red 4, green 16, blue 17, lit when
// the pin is LOW). On any other ESP32, wire an RGB LED and change the three pins and kActiveLow.
//
// In MyDesktopWidget: Settings > USB screens > Look for USB screens, then Save.
// SPDX-License-Identifier: MIT

#include <MyDesktopWidget.h>

const int kRed = 4, kGreen = 16, kBlue = 17;
const bool kActiveLow = true;

MyDesktopWidget mdw(Serial);

void show(int red, int green, int blue) {
  analogWrite(kRed, kActiveLow ? 255 - red : red);
  analogWrite(kGreen, kActiveLow ? 255 - green : green);
  analogWrite(kBlue, kActiveLow ? 255 - blue : blue);
}

void setup() {
  pinMode(kRed, OUTPUT);
  pinMode(kGreen, OUTPUT);
  pinMode(kBlue, OUTPUT);
  show(0, 0, 40);  // dim blue: waiting for the PC

  mdw.begin("RgbLed");

  mdw.subscribe("cpu.load.cpu_total", [](float load) {
    const int red = int(constrain(load, 0.0f, 100.0f) * 2.55f);
    show(red, 255 - red, 0);
  });

  // Absent is not zero: with no reading, say so rather than showing an idle green.
  mdw.onMissing("cpu.load.cpu_total", [] { show(0, 0, 40); });
  mdw.onConnection([](bool connected, const char*) { if (!connected) show(0, 0, 40); });
}

void loop() { mdw.loop(); }
