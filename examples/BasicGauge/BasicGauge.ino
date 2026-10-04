// BasicGauge - the PC's CPU load and memory on a Cheap Yellow Display (ESP32-2432S028R).
//
// Needs the TFT_eSPI library, set up for your panel. The Cheap Yellow Display comes with two
// different panels even under one part number, so if the screen stays white, try the other one -
// see extras/boards/README.md for both TFT_eSPI setups.
//
// In MyDesktopWidget: Settings > USB screens > Look for USB screens, then Save.
// SPDX-License-Identifier: MIT

#include <MyDesktopWidget.h>
#include <TFT_eSPI.h>

MyDesktopWidget mdw(Serial);
TFT_eSPI tft;

const int kBarX = 20, kBarWidth = 280, kBarHeight = 22;

// The last value drawn, or NAN for "no reading". Redrawn only when it changes.
float cpu = NAN, memory = NAN;
bool dirty = true;

void drawBar(int y, const char* label, float value) {
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(label, kBarX, y - 22, 2);

  char text[8];
  // Absent is not zero: a reading MyDesktopWidget cannot take is shown as "--", never as 0%.
  if (isnan(value)) snprintf(text, sizeof(text), "  --  ");
  else snprintf(text, sizeof(text), " %3.0f%% ", value);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(text, kBarX + kBarWidth, y - 22, 2);

  const int filled = isnan(value) ? 0 : int(constrain(value, 0.0f, 100.0f) / 100.0f * kBarWidth);
  tft.fillRect(kBarX, y, filled, kBarHeight, value > 85 ? TFT_RED : TFT_CYAN);
  tft.fillRect(kBarX + filled, y, kBarWidth - filled, kBarHeight, TFT_DARKGREY);
}

void draw() {
  if (!mdw.connected()) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Waiting for MyDesktopWidget", 160, 120, 2);
    return;
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(mdw.machine(), kBarX, 12, 4);

  drawBar(100, "CPU", cpu);
  drawBar(180, "Memory", memory);
}

void setup() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  mdw.setBoard("cyd-2432s028r");
  mdw.begin("BasicGauge");

  mdw.subscribe("cpu.load.cpu_total", [](float v) { if (v != cpu) { cpu = v; dirty = true; } });
  mdw.onMissing("cpu.load.cpu_total", [] { cpu = NAN; dirty = true; });

  mdw.subscribe("memory.load.memory", [](float v) { if (v != memory) { memory = v; dirty = true; } });
  mdw.onMissing("memory.load.memory", [] { memory = NAN; dirty = true; });

  mdw.onConnection([](bool, const char*) {
    tft.fillScreen(TFT_BLACK);
    dirty = true;
  });

  draw();
}

void loop() {
  mdw.loop();

  if (dirty) {
    dirty = false;
    draw();
  }
}
