// A stand-in for LVGL with only what MyDesktopWidgetLvgl.h calls, so its binding can be tested on a PC.
// Each widget remembers what it was told, and how many times its text was set.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>

struct lv_obj_class_t {
  int id;
};

inline const lv_obj_class_t lv_arc_class{1};
inline const lv_obj_class_t lv_bar_class{2};
inline const lv_obj_class_t lv_slider_class{3};
inline const lv_obj_class_t lv_label_class{4};

struct lv_obj_t {
  const lv_obj_class_t* cls;
  int32_t value = 0;
  std::string text;
  uint32_t state = 0;
  int textSets = 0;
};

#define LV_STATE_DISABLED 0x0080
#define LV_ANIM_OFF 0

inline bool lv_obj_check_type(const lv_obj_t* object, const lv_obj_class_t* cls) { return object->cls == cls; }
inline void lv_obj_add_state(lv_obj_t* object, uint32_t state) { object->state |= state; }
inline void lv_obj_clear_state(lv_obj_t* object, uint32_t state) { object->state &= ~state; }
inline void lv_arc_set_value(lv_obj_t* object, int32_t value) { object->value = value; }
inline void lv_bar_set_value(lv_obj_t* object, int32_t value, int) { object->value = value; }
inline void lv_slider_set_value(lv_obj_t* object, int32_t value, int) { object->value = value; }
inline const char* lv_label_get_text(const lv_obj_t* object) { return object->text.c_str(); }
inline void lv_label_set_text(lv_obj_t* object, const char* text) {
  object->text = text;
  ++object->textSets;
}
