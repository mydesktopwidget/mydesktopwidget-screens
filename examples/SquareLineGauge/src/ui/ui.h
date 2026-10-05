// Stands in for SquareLine Studio's export: every widget is a global, declared here. SquareLine writes
// this file; you rename widgets in the designer, never here. SPDX-License-Identifier: MIT

#ifndef _SQUARELINE_GAUGE_UI_H
#define _SQUARELINE_GAUGE_UI_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern lv_obj_t * ui_Main;
extern lv_obj_t * ui_mdw_cpu__load__cpu_total;
extern lv_obj_t * ui_mdw_cpu__load__cpu_total___label;
extern lv_obj_t * ui_mdw_memory__load__memory;

void ui_init(void);

#ifdef __cplusplus
}
#endif

#endif
