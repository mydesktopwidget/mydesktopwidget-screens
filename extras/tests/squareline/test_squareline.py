# Tests for extras/squareline/mdw_squareline.py, against a project shaped like a SquareLine export.
#
#   python -m unittest discover -s extras/tests/squareline
#
# SPDX-License-Identifier: MIT

import importlib.util
import os
import shutil
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, '..', '..', 'squareline', 'mdw_squareline.py')

spec = importlib.util.spec_from_file_location('mdw_squareline', SCRIPT)
squareline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(squareline)

# What SquareLine's export declares, as its documentation shows: every widget a global in ui.h, and since
# 1.4 each screen's widgets in a header of its own.
UI_H = '''#ifndef _UI_H
#define _UI_H
#include "lvgl.h"
#include "screens/ui_Main.h"
extern lv_obj_t * ui_Main;
extern lv_obj_t * ui_Title;
extern lv_obj_t * ui_mdw_cpu__load__cpu_total;
extern lv_obj_t * ui_mdw_cpu__load__cpu_total___label;
extern lv_obj_t * ui_mdw_CPU;
void ui_init(void);
#endif
'''

MAIN_H = '''#pragma once
extern lv_obj_t *ui_mdw_memory__load__memory;
extern lv_obj_t*ui_mdw_media__title;
extern lv_obj_t * ui_mdw_cpu__load__cpu_total;
'''


class SquareLineTests(unittest.TestCase):
    def setUp(self):
        self.src = tempfile.mkdtemp()
        os.makedirs(os.path.join(self.src, 'ui', 'screens'))
        with open(os.path.join(self.src, 'ui', 'ui.h'), 'w') as f:
            f.write(UI_H)
        with open(os.path.join(self.src, 'ui', 'screens', 'ui_Main.h'), 'w') as f:
            f.write(MAIN_H)

    def tearDown(self):
        shutil.rmtree(self.src)

    def test_a_dot_is_two_underscores_and_three_start_a_tag(self):
        self.assertEqual(squareline.reading_id('ui_mdw_cpu__load__cpu_total'), ('cpu.load.cpu_total', None))
        self.assertEqual(squareline.reading_id('ui_mdw_cpu__load__cpu_total___label')[0], 'cpu.load.cpu_total')
        self.assertEqual(squareline.reading_id('ui_mdw_cpu__clock__p_core_1')[0], 'cpu.clock.p_core_1')

    def test_a_name_that_is_not_an_id_is_named_in_a_warning_and_not_bound(self):
        for name in ('ui_mdw_CPU', 'ui_mdw_cpu', 'ui_mdw_cpu___tag', 'ui_mdw_cpu__load__'):
            rid, problem = squareline.reading_id(name)
            self.assertIsNone(rid, name)
            self.assertIn(name[3:], problem)

    def test_every_widget_in_every_exported_header_is_found_once(self):
        found, warnings = squareline.find(self.src)
        bound = {(name, rid) for _, name, rid in found}
        self.assertEqual(bound, {
            ('ui_mdw_cpu__load__cpu_total', 'cpu.load.cpu_total'),
            ('ui_mdw_cpu__load__cpu_total___label', 'cpu.load.cpu_total'),
            ('ui_mdw_memory__load__memory', 'memory.load.memory'),
            ('ui_mdw_media__title', 'media.title'),
        })
        self.assertEqual(len(found), 4)  # cpu_total is declared twice, and listed once
        self.assertEqual(len(warnings), 1)
        self.assertIn('mdw_CPU', warnings[0])

    def test_the_table_includes_the_headers_and_ends_even_when_empty(self):
        squareline.write(self.src)
        with open(os.path.join(self.src, squareline.OUTPUT_NAME)) as f:
            table = f.read()
        self.assertIn('#include "ui/ui.h"', table)
        self.assertIn('#include "ui/screens/ui_Main.h"', table)
        self.assertIn('MDW_LVGL(ui_mdw_media__title, "media.title")', table)
        self.assertIn('MDW_LVGL_END', table)

        empty = tempfile.mkdtemp()
        try:
            squareline.write(empty)
            with open(os.path.join(empty, squareline.OUTPUT_NAME)) as f:
                self.assertIn('MDW_LVGL_END', f.read())
        finally:
            shutil.rmtree(empty)

    def test_an_unchanged_export_does_not_rewrite_the_table(self):
        self.assertTrue(squareline.write(self.src)[2])
        self.assertFalse(squareline.write(self.src)[2])

        with open(os.path.join(self.src, 'ui', 'ui.h'), 'a') as f:
            f.write('extern lv_obj_t * ui_mdw_gpu__load__gpu;\n')
        self.assertTrue(squareline.write(self.src)[2])


if __name__ == '__main__':
    unittest.main()
