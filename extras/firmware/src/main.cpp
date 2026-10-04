// The ready firmware: MyDesktopWidget draws one of its skins on this screen.
//
// Pick the skin on the PC, in Settings > USB screens. Until one is picked - or while MyDesktopWidget
// is not running - the screen shows its own small page: the PC's name and its CPU and memory load.
// SPDX-License-Identifier: MIT

#include <Arduino.h>
#include <MyDesktopWidget.h>
#include <TFT_eSPI.h>

namespace {

const int kWidth = 320, kHeight = 240;

// With no tile for this long, the PC has no skin chosen for this screen; show the values page.
const uint32_t kNoPictureMs = 4000;

MyDesktopWidget mdw(Serial);
TFT_eSPI tft;

float cpu = NAN, memory = NAN;
uint32_t lastTileMs = 0;
bool showingPicture = false;
bool dirty = true;

void drawBar(int y, const char* label, float value) {
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(label, 20, y - 22, 2);

  char text[8];
  // Absent is not zero: a reading the PC cannot take shows "--", never 0%.
  if (isnan(value)) snprintf(text, sizeof(text), "  --  ");
  else snprintf(text, sizeof(text), " %3.0f%% ", value);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(text, 300, y - 22, 2);

  const int filled = isnan(value) ? 0 : int(constrain(value, 0.0f, 100.0f) * 2.8f);
  tft.fillRect(20, y, filled, 18, TFT_CYAN);
  tft.fillRect(20 + filled, y, 280 - filled, 18, TFT_DARKGREY);
}

void drawValuesPage() {
  tft.setTextDatum(TL_DATUM);

  if (!mdw.connected()) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Waiting for MyDesktopWidget", kWidth / 2, kHeight / 2, 2);
    return;
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(mdw.machine(), 20, 14, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Pick a skin in Settings > USB screens", 20, 210, 2);

  drawBar(100, "CPU", cpu);
  drawBar(160, "Memory", memory);
}

}  // namespace

void setup() {
  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);  // tiles arrive as RGB565 values; the panel wants their high byte first
  tft.fillScreen(TFT_BLACK);

  mdw.setBoard("cyd-2432s028r");
  mdw.setScreen(kWidth, kHeight);

  mdw.onTile([](uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* pixels) {
    if (!showingPicture) {
      showingPicture = true;  // the first tile of a picture: the values page is gone
    }
    lastTileMs = millis();
    tft.pushImage(x, y, w, h, const_cast<uint16_t*>(pixels));
  });

  mdw.subscribe("cpu.load.cpu_total", [](float v) { if (v != cpu) { cpu = v; dirty = true; } });
  mdw.onMissing("cpu.load.cpu_total", [] { cpu = NAN; dirty = true; });
  mdw.subscribe("memory.load.memory", [](float v) { if (v != memory) { memory = v; dirty = true; } });
  mdw.onMissing("memory.load.memory", [] { memory = NAN; dirty = true; });

  mdw.onConnection([](bool, const char*) {
    showingPicture = false;
    tft.fillScreen(TFT_BLACK);
    dirty = true;
  });

  mdw.begin("MyDesktopWidget Screen");
  drawValuesPage();
}

void loop() {
  mdw.loop();

  if (showingPicture && millis() - lastTileMs > kNoPictureMs && !mdw.showsImages()) {
    showingPicture = false;
    tft.fillScreen(TFT_BLACK);
    dirty = true;
  }

  // An unchanged picture sends nothing, so silence with a picture up is normal; the values page comes
  // back only when the picture has not started at all.
  if (!showingPicture && dirty) {
    dirty = false;
    drawValuesPage();
  }
}
