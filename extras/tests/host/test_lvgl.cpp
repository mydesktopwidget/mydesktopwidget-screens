// Off-device tests for MyDesktopWidgetLvgl.h, against the stand-in LVGL in fake_lvgl/. Built and run
// by run.ps1 beside test_client.cpp.
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <deque>
#include <string>

#include "MyDesktopWidgetLvgl.h"
#include "mdw/Client.h"

namespace {

int failures = 0;
int checks = 0;

#define CHECK(condition)                                                        \
  do {                                                                          \
    ++checks;                                                                   \
    if (!(condition)) {                                                         \
      ++failures;                                                               \
      std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);      \
    }                                                                           \
  } while (0)

class Port : public mdwlib::Port {
 public:
  std::deque<uint8_t> inbound;
  std::string lastSubscribe;

  size_t read(uint8_t* buffer, size_t capacity) override {
    size_t n = 0;
    while (n < capacity && !inbound.empty()) {
      buffer[n++] = inbound.front();
      inbound.pop_front();
    }
    return n;
  }

  void write(const uint8_t* data, size_t length) override {
    const std::string payload(reinterpret_cast<const char*>(data + 4), length - 4);
    if (payload.find(R"("type":"subscribe")") != std::string::npos) lastSubscribe = payload;
  }

  void frame(const std::string& json) {
    const size_t n = json.size();
    for (int shift = 24; shift >= 0; shift -= 8) inbound.push_back(uint8_t((n >> shift) & 0xFF));
    for (char c : json) inbound.push_back(uint8_t(c));
  }
};

std::string telemetry(const char* readings, const char* text = "{}") {
  return std::string(R"({"v":1,"type":"telemetry","at":1,"readings":)") + readings + R"(,"text":)" + text + "}";
}

// What SquareLine's ui_init() creates; null until then, as the export's globals are.
lv_obj_t* ui_mdw_cpu__load__cpu_total = nullptr;
lv_obj_t* ui_mdw_cpu__load__cpu_total___label = nullptr;
lv_obj_t* ui_mdw_memory__load__memory = nullptr;
lv_obj_t* ui_mdw_media__title = nullptr;

lv_obj_t cpuArc{&lv_arc_class};
lv_obj_t cpuLabel{&lv_label_class};
lv_obj_t memoryBar{&lv_bar_class};
lv_obj_t titleLabel{&lv_label_class};

const mdwlib::LvglBinding bindings[] = {
    MDW_LVGL(ui_mdw_cpu__load__cpu_total, "cpu.load.cpu_total")
    MDW_LVGL(ui_mdw_cpu__load__cpu_total___label, "cpu.load.cpu_total")
    MDW_LVGL(ui_mdw_memory__load__memory, "memory.load.memory")
    MDW_LVGL(ui_mdw_media__title, "media.title")
    MDW_LVGL_END
};

struct Rig {
  Port port;
  mdwlib::Client client{port};
  uint32_t now = 1000;

  Rig() { client.setIdentity("3C61053ED814", "LVGL", nullptr, "test"); }

  void connect() {
    client.loop(now);
    port.frame(R"({"v":1,"type":"ready","machine":"DESK-PC","commands":[],"modes":["values"]})");
    client.loop(now += 10);
  }

  void send(const std::string& json) {
    port.frame(json);
    client.loop(now += 10);
  }
};

/// One reading drives every widget bound to it - an arc and its number - and is asked for once.
void eachReadingDrivesEveryWidgetBoundToIt() {
  Rig rig;
  CHECK(mdwBindLvgl(rig.client, bindings) == 3);  // three readings, four widgets

  ui_mdw_cpu__load__cpu_total = &cpuArc;
  ui_mdw_cpu__load__cpu_total___label = &cpuLabel;
  ui_mdw_memory__load__memory = &memoryBar;
  ui_mdw_media__title = &titleLabel;

  rig.connect();
  CHECK(rig.port.lastSubscribe.find("cpu.load.cpu_total") == rig.port.lastSubscribe.rfind("cpu.load.cpu_total"));

  rig.send(telemetry(R"({"cpu.load.cpu_total":37.4,"memory.load.memory":61.6})", R"({"media.title":"Clair de lune"})"));
  CHECK(cpuArc.value == 37);
  CHECK(cpuLabel.text == "37.4");
  CHECK(memoryBar.value == 62);
  CHECK(titleLabel.text == "Clair de lune");
}

/// Absent is never 0: a missing reading disables the widget, and a label says "--".
void aMissingReadingDisablesItsWidgetsAndALabelSaysSo() {
  Rig rig;
  mdwBindLvgl(rig.client, bindings);
  rig.connect();

  rig.send(telemetry(R"({"cpu.load.cpu_total":12})"));
  CHECK((cpuArc.state & LV_STATE_DISABLED) == 0);

  rig.send(telemetry("{}"));
  CHECK((cpuArc.state & LV_STATE_DISABLED) != 0);
  CHECK(cpuArc.value == 12);  // left where it was, not dropped to 0
  CHECK(cpuLabel.text == "--");
  CHECK((cpuLabel.state & LV_STATE_DISABLED) != 0);

  rig.send(telemetry(R"({"cpu.load.cpu_total":50})"));
  CHECK((cpuArc.state & LV_STATE_DISABLED) == 0);
  CHECK(cpuLabel.text == "50");
}

/// A label is set only when its text changes: twice a second, for ~20 labels, the redraw shows.
void aLabelIsSetOnlyWhenItsTextChanges() {
  Rig rig;
  mdwBindLvgl(rig.client, bindings);
  rig.connect();

  rig.send(telemetry(R"({"cpu.load.cpu_total":44})"));
  const int sets = cpuLabel.textSets;
  rig.send(telemetry(R"({"cpu.load.cpu_total":44.01})"));
  rig.send(telemetry(R"({"cpu.load.cpu_total":44})"));
  CHECK(cpuLabel.textSets == sets);

  rig.send(telemetry(R"({"cpu.load.cpu_total":45})"));
  CHECK(cpuLabel.textSets == sets + 1);
}

void numbersAreFormattedForALabel() {
  char text[24];
  mdwlib::lvgl::format(37.0f, text, sizeof(text));
  CHECK(std::string(text) == "37");
  mdwlib::lvgl::format(3.46f, text, sizeof(text));
  CHECK(std::string(text) == "3.5");
  mdwlib::lvgl::format(1450.6f, text, sizeof(text));
  CHECK(std::string(text) == "1451");
  mdwlib::lvgl::format(-2.25f, text, sizeof(text));
  CHECK(std::string(text) == "-2.2" || std::string(text) == "-2.3");
}

/// Before ui_init() the widgets do not exist; a reading that arrives then is not a crash.
void aReadingBeforeTheWidgetsExistIsIgnored() {
  ui_mdw_media__title = nullptr;
  Rig rig;
  mdwBindLvgl(rig.client, bindings);
  rig.connect();
  rig.send(telemetry("{}", R"({"media.title":"Gymnopedie"})"));
  ui_mdw_media__title = &titleLabel;
  CHECK(titleLabel.text != "Gymnopedie");
}

/// A table that binds nothing is still a table; a family is not one reading and is not bound.
void anEmptyTableAndAFamilyBindNothing() {
  Rig rig;
  const mdwlib::LvglBinding empty[] = {MDW_LVGL_END};
  CHECK(mdwBindLvgl(rig.client, empty) == 0);

  lv_obj_t* family = &cpuArc;
  const mdwlib::LvglBinding families[] = {MDW_LVGL(family, "cpu.load.cpu_core_*") MDW_LVGL_END};
  CHECK(mdwBindLvgl(rig.client, families) == 0);
}

}  // namespace

int main() {
  struct Test {
    const char* name;
    void (*run)();
  } tests[] = {
      {"each reading drives every widget bound to it", eachReadingDrivesEveryWidgetBoundToIt},
      {"a missing reading disables its widgets", aMissingReadingDisablesItsWidgetsAndALabelSaysSo},
      {"a label is set only when its text changes", aLabelIsSetOnlyWhenItsTextChanges},
      {"numbers are formatted for a label", numbersAreFormattedForALabel},
      {"a reading before the widgets exist is ignored", aReadingBeforeTheWidgetsExistIsIgnored},
      {"an empty table and a family bind nothing", anEmptyTableAndAFamilyBindNothing},
  };

  for (const Test& test : tests) {
    const int before = failures;
    test.run();
    std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", test.name);
  }

  std::printf("\n%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
