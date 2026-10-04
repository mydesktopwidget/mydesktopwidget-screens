// CoreBars - one bar per CPU core, from a single family subscription.
//
// "cpu.load.cpu_core_*" asks for every reading whose id starts with "cpu.load.cpu_core_", however
// many cores the PC has. The callback runs once per core on every update, with the core's own id.
//
// Needs TFT_eSPI set up for your panel; see BasicGauge and extras/boards/README.md.
// SPDX-License-Identifier: MIT

#include <MyDesktopWidget.h>
#include <TFT_eSPI.h>

MyDesktopWidget mdw(Serial);
TFT_eSPI tft;

const int kMaxCores = 32;
float load[kMaxCores];
int cores = 0;
bool dirty = true;

void draw() {
  tft.fillScreen(TFT_BLACK);

  if (!mdw.connected() || cores == 0) {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(mdw.connected() ? "No per-core readings" : "Waiting for MyDesktopWidget", 160, 120, 2);
    return;
  }

  const int width = 300 / cores;
  for (int i = 0; i < cores; ++i) {
    const int height = int(constrain(load[i], 0.0f, 100.0f) * 2.0f);
    tft.fillRect(10 + i * width, 220 - height, width - 2, height, load[i] > 85 ? TFT_RED : TFT_GREEN);
  }
}

void setup() {
  tft.init();
  tft.setRotation(1);

  mdw.begin("CoreBars");

  mdw.subscribe("cpu.load.cpu_core_*", [](const char* id, float value) {
    // The ids are numbered from 1: cpu.load.cpu_core_1, cpu.load.cpu_core_2, ...
    const int core = atoi(id + strlen("cpu.load.cpu_core_")) - 1;
    if (core < 0 || core >= kMaxCores) return;
    if (core + 1 > cores) cores = core + 1;
    if (load[core] != value) {
      load[core] = value;
      dirty = true;
    }
  });

  mdw.onConnection([](bool, const char*) {
    cores = 0;
    dirty = true;
  });
}

void loop() {
  mdw.loop();

  // At most ten redraws a second: a family arrives as one callback per core, and drawing after each
  // one would repaint the screen a dozen times per update.
  static uint32_t lastDraw = 0;
  if (dirty && millis() - lastDraw > 100) {
    dirty = false;
    lastDraw = millis();
    draw();
  }
}
