// MyDesktopWidget Screens - binding an EEZ Studio screen to the PC's readings.
// SPDX-License-Identifier: MIT
//
// In EEZ Studio, make a native variable for each reading - type float for a number, string for words -
// and bind your widgets to it. Then, in one .cpp or .ino file of your sketch:
//
//   #include <MyDesktopWidget.h>
//   MyDesktopWidget mdw(Serial);
//
//   MDW_EEZ_FLOAT(cpu_load, "cpu.load.cpu_total")   // the EEZ variable cpu_load
//   MDW_EEZ_TEXT(media_title, "media.title")        // the EEZ variable media_title
//
// Each line writes the get_var_... and set_var_... functions EEZ's generated vars.h asks for, so there is
// no binding code of your own to write. A reading the PC cannot take is NAN for a number and "" for words:
// absent is not zero, so pick a display format in EEZ that shows NAN as "--".
//
// The setter does nothing on purpose: a reading belongs to the PC, and a screen does not write it back.
// The instance is `mdw`; define MDW_EEZ_INSTANCE before including the library to use another name.

#pragma once

#ifndef MDW_EEZ_INSTANCE
#define MDW_EEZ_INSTANCE mdw
#endif

// C linkage, because EEZ generates its screens as C files that call these. The reading is watched on the
// first call rather than at start-up, so the order the sketch's globals are built in does not matter.
#define MDW_EEZ_FLOAT(name, id)                                                     \
  extern "C" float get_var_##name() {                                               \
    MDW_EEZ_INSTANCE.watch(id);                                                     \
    return MDW_EEZ_INSTANCE.last(id);                                               \
  }                                                                                 \
  extern "C" void set_var_##name(float) {}

#define MDW_EEZ_TEXT(name, id)                                                      \
  extern "C" const char* get_var_##name() {                                         \
    MDW_EEZ_INSTANCE.watch(id);                                                     \
    return MDW_EEZ_INSTANCE.lastText(id);                                           \
  }                                                                                 \
  extern "C" void set_var_##name(const char*) {}
