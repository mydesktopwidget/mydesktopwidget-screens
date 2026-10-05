// MyDesktopWidget Screens - LVGL widgets that show the PC's readings, with no binding code of your own.
// SPDX-License-Identifier: MIT
//
// Designed in SquareLine Studio: name a widget `mdw_` followed by the reading's id, writing each dot as
// a double underscore - `mdw_cpu__load__cpu_total` shows `cpu.load.cpu_total`. Two widgets showing one
// reading (an arc and its number) need two names: add three underscores and anything after them -
// `mdw_cpu__load__cpu_total___label`. Add one line to platformio.ini:
//
//   extra_scripts = pre:.pio/libdeps/${PIOENV}/MyDesktopWidget/extras/squareline/mdw_squareline.py
//
// and the build writes src/mdw_bindings.generated.h, listing every such widget. Then:
//
//   #include "mdw_bindings.generated.h"
//   ...
//   ui_init();
//   mdwBindLvgl(mdw, mdwLvglBindings);
//
// Arduino IDE has no build scripts, so write the list yourself - the same list the script writes:
//
//   static const mdwlib::LvglBinding mdwLvglBindings[] = {
//     MDW_LVGL(ui_CpuArc, "cpu.load.cpu_total")
//     MDW_LVGL(ui_MemoryLabel, "memory.load.memory")
//     MDW_LVGL_END
//   };
//
// What each widget is given: an arc, bar or slider the value (set its range in the designer - loads are
// 0 to 100); a label the number, or the words for a reading that is words (a media title). A reading
// the PC cannot take puts the widget in LV_STATE_DISABLED - style that state to grey it out - and a label
// shows "--". Absent is never shown as 0.
//
// Call it after ui_init() and before the first mdw.loop(); LVGL is only touched from mdw.loop().

#pragma once

#include <lvgl.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace mdwlib {

/// One widget and the reading it shows. The widget is the address of SquareLine's `ui_...` global,
/// read when a value arrives, so the table can be built before ui_init() creates the widgets.
struct LvglBinding {
  lv_obj_t** object;
  const char* id;
};

namespace lvgl {

/// A number as a label shows it: whole numbers and anything of 100 or more without decimals, the rest
/// with one - "37", "1450", "3.5".
inline void format(float value, char* text, size_t size) {
  const bool whole = std::fabs(value - std::round(value)) < 0.05f || std::fabs(value) >= 100.0f;
  std::snprintf(text, size, whole ? "%.0f" : "%.1f", static_cast<double>(value));
}

/// Sets a label's text only when it changed: redrawing a label costs, and readings come twice a second.
inline void setText(lv_obj_t* label, const char* text) {
  const char* now = lv_label_get_text(label);
  if (now == nullptr || std::strcmp(now, text) != 0) lv_label_set_text(label, text);
}

inline void showValue(lv_obj_t* object, float value) {
  if (object == nullptr) return;
  lv_obj_clear_state(object, LV_STATE_DISABLED);

  const auto whole = static_cast<int32_t>(std::lround(value));
  if (lv_obj_check_type(object, &lv_arc_class)) {
    lv_arc_set_value(object, whole);
  } else if (lv_obj_check_type(object, &lv_bar_class)) {
    lv_bar_set_value(object, whole, LV_ANIM_OFF);
  } else if (lv_obj_check_type(object, &lv_slider_class)) {
    lv_slider_set_value(object, whole, LV_ANIM_OFF);
  } else if (lv_obj_check_type(object, &lv_label_class)) {
    char text[24];
    format(value, text, sizeof(text));
    setText(object, text);
  }
}

inline void showText(lv_obj_t* object, const char* text) {
  if (object == nullptr) return;
  lv_obj_clear_state(object, LV_STATE_DISABLED);
  if (lv_obj_check_type(object, &lv_label_class)) setText(object, text);
}

inline void showMissing(lv_obj_t* object) {
  if (object == nullptr) return;
  lv_obj_add_state(object, LV_STATE_DISABLED);
  if (lv_obj_check_type(object, &lv_label_class)) setText(object, "--");
}

/// Every widget in the table bound to `id` - one reading may drive an arc and its label together.
struct Widgets {
  const LvglBinding* table;
  size_t count;
  const char* id;

  template <class Show, class Argument>
  void each(Show show, Argument argument) const {
    for (size_t i = 0; i < count; ++i) {
      if (table[i].object != nullptr && table[i].id != nullptr && std::strcmp(table[i].id, id) == 0) {
        show(*table[i].object, argument);
      }
    }
  }
};

inline void missing(lv_obj_t* object, int) { showMissing(object); }

}  // namespace lvgl
}  // namespace mdwlib

/// One line of a binding table: the widget's `ui_...` name and the reading's id.
#define MDW_LVGL(object, id) {&(object), (id)},

/// Ends a binding table, so a table is valid even when it binds nothing.
#define MDW_LVGL_END {nullptr, nullptr}

/// Binds every widget in the table to its reading, and returns how many readings were bound. The table
/// must live as long as the sketch - a global or static array, as the generated one is. A reading is
/// skipped when the subscription list is full (MDW_MAX_SUBSCRIPTIONS) or its id is a family.
template <class Source, size_t N>
size_t mdwBindLvgl(Source& source, const mdwlib::LvglBinding (&bindings)[N]) {
  size_t bound = 0;

  for (size_t i = 0; i < N; ++i) {
    const char* id = bindings[i].id;
    if (bindings[i].object == nullptr || id == nullptr) continue;

    // Once per reading: the callbacks of a later widget for the same id would replace these.
    bool earlier = false;
    for (size_t j = 0; j < i && !earlier; ++j) {
      earlier = bindings[j].id != nullptr && std::strcmp(bindings[j].id, id) == 0;
    }
    if (earlier) continue;

    const mdwlib::lvgl::Widgets widgets{bindings, N, id};
    const bool ok =
        source.subscribe(id, [widgets](float value) { widgets.each(mdwlib::lvgl::showValue, value); }) &&
        source.onText(id, [widgets](const char* text) { widgets.each(mdwlib::lvgl::showText, text); }) &&
        source.onMissing(id, [widgets] { widgets.each(mdwlib::lvgl::missing, 0); });
    if (ok) ++bound;
  }

  return bound;
}
