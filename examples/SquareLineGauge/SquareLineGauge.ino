// SquareLineGauge - a screen designed in SquareLine Studio, showing the PC's readings with no binding code.
//
// The screen in src/ui/ stands in for a SquareLine export (LVGL 8.3): an arc and a label named after the
// CPU load, and a bar named after memory. In SquareLine you would only name them:
//
//   mdw_cpu__load__cpu_total          the arc   (each dot in the id is two underscores)
//   mdw_cpu__load__cpu_total___label  its label (three underscores and a tag: a second widget, same reading)
//   mdw_memory__load__memory          the bar
//
// With PlatformIO, one line in platformio.ini writes the list below for you on every build - see
// MyDesktopWidgetLvgl.h. The Arduino IDE has no build scripts, so here it is written by hand.
//
// Needs LVGL 8.3 (with an lv_conf.h, as LVGL's Arduino guide says) and TFT_eSPI set up for your panel -
// see extras/boards/README.md. In MyDesktopWidget: Settings > USB screens, switch them on, then Save.
// SPDX-License-Identifier: MIT

#include <MyDesktopWidget.h>
#include <MyDesktopWidgetLvgl.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "src/ui/ui.h"

MyDesktopWidget mdw(Serial);
TFT_eSPI tft;

static const mdwlib::LvglBinding bindings[] = {
    MDW_LVGL(ui_mdw_cpu__load__cpu_total, "cpu.load.cpu_total")
    MDW_LVGL(ui_mdw_cpu__load__cpu_total___label, "cpu.load.cpu_total")
    MDW_LVGL(ui_mdw_memory__load__memory, "memory.load.memory")
    MDW_LVGL_END
};

const uint16_t kWidth = 320, kHeight = 240;
static lv_disp_draw_buf_t drawBuffer;
static lv_color_t pixels[kWidth * 20];
static uint32_t lastTick = 0;

// LVGL draws into a buffer; this hands each finished part to the panel.
void flush(lv_disp_drv_t* display, const lv_area_t* area, lv_color_t* colours) {
  const uint32_t w = area->x2 - area->x1 + 1, h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(reinterpret_cast<uint16_t*>(&colours->full), w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(display);
}

void setup() {
  tft.init();
  tft.setRotation(1);

  lv_init();
  lv_disp_draw_buf_init(&drawBuffer, pixels, nullptr, kWidth * 20);
  static lv_disp_drv_t display;
  lv_disp_drv_init(&display);
  display.hor_res = kWidth;
  display.ver_res = kHeight;
  display.flush_cb = flush;
  display.draw_buf = &drawBuffer;
  lv_disp_drv_register(&display);

  ui_init();

  // After ui_init(), which creates the widgets; before the first mdw.loop(), which feeds them.
  mdwBindLvgl(mdw, bindings);
  mdw.begin("SquareLine gauge");
}

void loop() {
  mdw.loop();

  const uint32_t now = millis();
  lv_tick_inc(now - lastTick);
  lastTick = now;
  lv_timer_handler();
}
