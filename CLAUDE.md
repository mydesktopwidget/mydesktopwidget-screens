# CLAUDE.md

**MyDesktopWidget Screens**: live PC stats from MyDesktopWidget on an ESP32 screen. An Arduino /
PlatformIO library (`src/`), example sketches (`examples/`), board profiles (`extras/boards/`) and a
ready firmware (`extras/firmware/`).

## ⚠ This folder is published as-is to a PUBLIC repository

It is developed inside the maintainer's private repository and mirrored to
`github.com/mydesktopwidget/mydesktopwidget-screens` (MIT) by `extras/maintainer/publish-mirror.ps1`,
which pushes only this folder's history.

- **Nothing in this folder may name anything private**: no internal host names, no paths on the
  maintainer's machine, no other folders of the private repository, no keys or tokens. The publish
  script refuses a tree that mentions the private repository's remote host, name or path, but that is
  a backstop, not a licence.
- **Plans, decisions and internal notes live outside this folder**, in the private repository's
  documents. This file and the READMEs are written for the public.
- **Only committed work is published**, and publishing is a deliberate step: commit, then run the
  script (`-DryRun` first).
- **Every public commit is Lucas Riechelmann Ramos's alone**: the script authors it under his GitHub
  no-reply address and strips any `Co-Authored-By:` line. No commit message, release note or registry
  listing for this library names a co-author.

## Rules

- **The repository root is the Arduino library.** Library Manager requires `library.properties` at the
  root, so the library is `src/` and `examples/` there, and everything that is not the library goes
  under `extras/` (the Arduino convention). `library.json` excludes the firmware and the maintainer
  scripts from the PlatformIO package.
- **Keep `library.properties` and `library.json` at the same version**, and the header's
  `MYDESKTOPWIDGET_VERSION` with them.
- **The protocol the library speaks is specified by MyDesktopWidget**, and where the specification
  and the engine disagree, the engine is right. The library must bound every frame before allocating
  it, treat an absent reading as "no value" (never zero), and re-send its subscription on every
  connect.
- **The one official board is the Cheap Yellow Display (ESP32-2432S028R).** Official means tested on
  real hardware; anything else is a community profile.
