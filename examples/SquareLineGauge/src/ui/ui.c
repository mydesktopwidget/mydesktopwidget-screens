// Stands in for SquareLine Studio's export (LVGL 8.3): one screen, an arc with its label, and a bar.
// The disabled state is styled faint - MyDesktopWidget puts a widget in it while its reading is missing.
// SPDX-License-Identifier: MIT

#include "ui.h"

lv_obj_t * ui_Main;
lv_obj_t * ui_mdw_cpu__load__cpu_total;
lv_obj_t * ui_mdw_cpu__load__cpu_total___label;
lv_obj_t * ui_mdw_memory__load__memory;

static void faint_when_disabled(lv_obj_t * obj)
{
    lv_obj_set_style_opa(obj, LV_OPA_40, LV_PART_MAIN | LV_STATE_DISABLED);
}

static void ui_Main_screen_init(void)
{
    ui_Main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Main, lv_color_black(), LV_PART_MAIN);

    ui_mdw_cpu__load__cpu_total = lv_arc_create(ui_Main);
    lv_obj_set_size(ui_mdw_cpu__load__cpu_total, 150, 150);
    lv_obj_align(ui_mdw_cpu__load__cpu_total, LV_ALIGN_LEFT_MID, 12, -10);
    lv_arc_set_range(ui_mdw_cpu__load__cpu_total, 0, 100);
    lv_arc_set_value(ui_mdw_cpu__load__cpu_total, 0);
    faint_when_disabled(ui_mdw_cpu__load__cpu_total);

    ui_mdw_cpu__load__cpu_total___label = lv_label_create(ui_Main);
    lv_label_set_text(ui_mdw_cpu__load__cpu_total___label, "--");
    lv_obj_set_style_text_color(ui_mdw_cpu__load__cpu_total___label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(ui_mdw_cpu__load__cpu_total___label, ui_mdw_cpu__load__cpu_total, LV_ALIGN_CENTER, 0, 0);
    faint_when_disabled(ui_mdw_cpu__load__cpu_total___label);

    ui_mdw_memory__load__memory = lv_bar_create(ui_Main);
    lv_obj_set_size(ui_mdw_memory__load__memory, 120, 20);
    lv_obj_align(ui_mdw_memory__load__memory, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_bar_set_range(ui_mdw_memory__load__memory, 0, 100);
    faint_when_disabled(ui_mdw_memory__load__memory);
}

void ui_init(void)
{
    ui_Main_screen_init();
    lv_disp_load_scr(ui_Main);
}
