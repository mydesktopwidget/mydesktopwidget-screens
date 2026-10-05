# MyDesktopWidget Screens - finds the SquareLine Studio widgets named after readings, and lists them.
# SPDX-License-Identifier: MIT
#
# PlatformIO runs this before every build when platformio.ini says:
#
#   extra_scripts = pre:.pio/libdeps/${PIOENV}/MyDesktopWidget/extras/squareline/mdw_squareline.py
#
# It reads the headers SquareLine exported into the project, finds every widget declared as
# `extern lv_obj_t * ui_mdw_...;`, and writes src/mdw_bindings.generated.h - the table mdwBindLvgl() takes.
# The file is rewritten only when the list changes, so it does not make every build a full one.
#
# Naming, in SquareLine: `mdw_` + the reading's id with each dot written `__` (two underscores), and
# optionally `___` (three) + any tag, so two widgets can show one reading:
#
#   mdw_cpu__load__cpu_total          ->  cpu.load.cpu_total
#   mdw_cpu__load__cpu_total___label  ->  cpu.load.cpu_total, a second widget
#
# It also runs on its own:  python mdw_squareline.py <project src folder> [output file]

import os
import re
import sys

DECLARATION = re.compile(r'\bextern\s+lv_obj_t\s*\*\s*(ui_mdw_[A-Za-z0-9_]+)\s*;')
READING_ID = re.compile(r'^[a-z0-9]+(?:_[a-z0-9]+)*(?:\.[a-z0-9]+(?:_[a-z0-9]+)*)+$')
OUTPUT_NAME = 'mdw_bindings.generated.h'


def reading_id(name):
    """The reading a widget name asks for, or None with the reason it cannot be one."""
    encoded = name[len('ui_mdw_'):].split('___', 1)[0]
    decoded = encoded.replace('__', '.')
    if not READING_ID.match(decoded):
        return None, ('"%s" does not name a reading: after mdw_, write the id in lower case with each dot '
                      'as two underscores, such as mdw_cpu__load__cpu_total' % name[len('ui_'):])
    return decoded, None


def find(src):
    """Every (header, widget, id) in the headers under src, and the warnings for names that are not ids."""
    found, warnings, seen = [], [], set()
    for folder, folders, files in os.walk(src):
        folders[:] = sorted(f for f in folders if not f.startswith('.'))
        for file in sorted(files):
            if not file.endswith('.h') or file == OUTPUT_NAME:
                continue
            path = os.path.join(folder, file)
            with open(path, encoding='utf-8', errors='replace') as header:
                text = header.read()
            for name in DECLARATION.findall(text):
                if name in seen:
                    continue
                seen.add(name)
                rid, problem = reading_id(name)
                if problem:
                    warnings.append(problem)
                else:
                    found.append((os.path.relpath(path, src).replace(os.sep, '/'), name, rid))
    return found, warnings


def render(found):
    headers = sorted({header for header, _, _ in found})
    lines = [
        '// Written by MyDesktopWidget\'s mdw_squareline.py from the SquareLine export. Do not edit:',
        '// rename the widget in SquareLine instead. SPDX-License-Identifier: MIT',
        '#pragma once',
        '',
    ]
    lines += ['#include "%s"' % header for header in headers]
    lines += [
        '#include <MyDesktopWidgetLvgl.h>',
        '',
        'static const mdwlib::LvglBinding mdwLvglBindings[] = {',
    ]
    lines += ['    MDW_LVGL(%s, "%s")' % (name, rid) for _, name, rid in found]
    lines += ['    MDW_LVGL_END', '};', '']
    return '\n'.join(lines)


def write(src, output=None):
    output = output or os.path.join(src, OUTPUT_NAME)
    found, warnings = find(src)
    for warning in warnings:
        print('MyDesktopWidget: ' + warning)
    content = render(found)
    try:
        with open(output, encoding='utf-8') as existing:
            if existing.read() == content:
                return found, warnings, False
    except OSError:
        pass
    with open(output, 'w', encoding='utf-8', newline='\n') as out:
        out.write(content)
    print('MyDesktopWidget: %d SquareLine widget(s) bound to readings, in %s' % (len(found), output))
    return found, warnings, True


try:
    Import('env')  # noqa: F821 - defined when PlatformIO runs this as an extra script
    write(env.subst('$PROJECT_SRC_DIR'))  # noqa: F821
except NameError:
    if __name__ == '__main__':
        if len(sys.argv) < 2:
            sys.exit('usage: python mdw_squareline.py <project src folder> [output file]')
        write(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
