# MyDesktopWidget Screens

Show live PC stats from [MyDesktopWidget](https://mydesktopwidget.com) on an ESP32 screen: CPU, GPU,
temperatures, network and media.

```cpp
#include <MyDesktopWidget.h>

MyDesktopWidget mdw(Serial);

void setup() {
  mdw.begin("Desk gauge");
  mdw.subscribe("cpu.load.cpu_total", [](float load) { /* draw it */ });
  mdw.onMissing("cpu.load.cpu_total", [] { /* draw "--" */ });
}

void loop() { mdw.loop(); }
```

> **Version 0.3.0.** The library works end to end on the Cheap Yellow Display, values and pictures. It is not yet in the
> Arduino Library Manager or the PlatformIO registry, and it needs the first MyDesktopWidget release
> with USB screens.

## How it works

MyDesktopWidget runs on your Windows PC and reads its sensors. Plug an ESP32 into the PC over USB, and
the engine sends it the readings your sketch asks for, twice a second. What the board does with them
is up to you.

| Level | For | You get |
|---|---|---|
| **1. Data** | Anyone who wants full control | This library. Subscribe to sensors and draw them however you like: TFT_eSPI, LVGL, LEDs, a servo, anything |
| **2. Designed screens** | Makers who prefer a visual designer | *Planned:* design an LVGL screen in SquareLine Studio or EEZ Studio and name its widgets after sensors |
| **3. Plug and play** | Beginners | *Planned:* a ready firmware flashed from the website, showing MyDesktopWidget's own skins |

## Getting started

1. **In MyDesktopWidget** on the PC: *Settings > USB screens > Look for USB screens*, then *Save*.
   Nothing serial is opened until you do this.
2. **Install the library** and [ArduinoJson](https://arduinojson.org) 7. Until it is in the Library
   Manager, copy this repository into your Arduino `libraries` folder, or in PlatformIO add its Git
   URL to `lib_deps`.
3. **Set up your display library.** For the Cheap Yellow Display, see
   [extras/boards](extras/boards/README.md): it ships with two different panels, and each needs its
   own TFT_eSPI setup.
4. **Upload [`examples/BasicGauge`](examples/BasicGauge/BasicGauge.ino).** The screen shows "Waiting
   for MyDesktopWidget", then the PC's name with its CPU and memory load.

The screen appears in *Settings > USB screens* with its port, board and library version.

## Three things to know

- **The serial port belongs to the library.** Do not `Serial.print` from your sketch: the PC would
  read your text as noise between messages.
- **Absent is not zero.** A reading the PC cannot take (a GPU temperature on a PC with no driver for
  it) is not sent as 0. `onMissing` tells you, so you can show `--` instead of a reassuring 0.
- **Uploading while MyDesktopWidget is running** fails with "port busy", because the engine has the
  port open. Press *Release port* beside the screen in *Settings > USB screens*, upload, then press
  *Look again*.

## The API

| Call | What it does |
|---|---|
| `MyDesktopWidget mdw(Serial)` | Uses the board's USB serial port. |
| `mdw.begin(name)` | Starts the port at 921,600 baud and starts announcing this screen as `name`. |
| `mdw.setBoard(id)` | Optional, before `begin`: a [board profile](extras/boards) id. |
| `mdw.subscribe(id, [](float v) { })` | Calls back with the reading on every update (twice a second). |
| `mdw.subscribe("prefix*", [](const char* id, float v) { })` | A family: once per matching reading, such as every CPU core with `cpu.load.cpu_core_*`. |
| `mdw.onMissing(id, [] { })` | Calls back once when a reading has no value. |
| `mdw.onText(id, [](const char* text) { })` | For readings whose value is words, such as a media title. |
| `mdw.watch(id)`, `mdw.last(id)`, `mdw.lastText(id)` | Ask for a reading with no callback, then read its last value whenever you draw: `NAN` (or `""`) while it has none, never 0. |
| `MDW_EEZ_FLOAT(variable, id)`, `MDW_EEZ_TEXT(variable, id)` | Binds an **EEZ Studio** native variable to a reading: one line each, no binding code. See `src/mdw/Eez.h`. *Written from EEZ's documentation; not yet tried in EEZ Studio itself.* |
| `mdw.onConnection([](bool connected, const char* machine) { })` | MyDesktopWidget came or went. It counts as gone after 5 seconds without a message. |
| `mdw.setScreen(width, height)` | Optional, before `begin`: this board has a screen, so MyDesktopWidget may draw one of its skins on it. |
| `mdw.onTile([](uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* pixels) { })` | A piece of that picture, as RGB565 values, to draw at `x, y`. With TFT_eSPI: `tft.setSwapBytes(true)` once, then `tft.pushImage(x, y, w, h, pixels)`. |
| `mdw.showsImages()` | Whether MyDesktopWidget agreed to send pictures. |
| `mdw.loop()` | Call it from `loop()`, often. It never blocks. |
| `mdw.connected()`, `mdw.machine()`, `mdw.refusal()`, `mdw.stats()` | The link's state, the PC's name, why the PC refused this screen (if it did), and counters. |

Up to 16 subscriptions (`MDW_MAX_SUBSCRIPTIONS`), and frames up to 8 KB are kept (`MDW_MAX_FRAME`).
To change either, set it as a **build flag** (PlatformIO: `build_flags = -DMDW_MAX_SUBSCRIPTIONS=32`),
never with a `#define` in your sketch: the library is compiled separately, and the two would disagree
about the size of its memory.

## Examples

| Example | Shows |
|---|---|
| [BasicGauge](examples/BasicGauge/BasicGauge.ino) | CPU and memory as bars on a Cheap Yellow Display, with "--" and a waiting screen |
| [RgbLed](examples/RgbLed/RgbLed.ino) | No screen at all: the board's RGB LED goes from green to red with CPU load |
| [CoreBars](examples/CoreBars/CoreBars.ino) | One bar per CPU core, from a single family subscription |

## What is in this repository

| Path | What |
|---|---|
| `src/` | The library. `src/mdw/` is the protocol itself, in plain C++ with no Arduino dependency |
| `examples/` | The example sketches |
| `extras/boards/` | Board profiles, their schema, and display setups |
| `extras/tests/` | Off-device tests of the protocol (`host/run.ps1`) and a compile check of every example |
| `extras/firmware/` | The ready firmware: flash it, then pick a skin for the screen in MyDesktopWidget. Until one is picked it shows CPU and memory |

## Supported hardware

**Officially tested:** the Cheap Yellow Display, ESP32-2432S028R (2.8" 320×240, resistive touch), with
the ILI9341 panel. Any ESP32 with a USB-serial chip MyDesktopWidget recognises (CH340, CP210x, or an
ESP32-S3's own USB) can run the library; the screen is your sketch's business.

## Requirements

- Windows with [MyDesktopWidget](https://mydesktopwidget.com) installed. This library works with the
  free version.
- An ESP32 connected by USB.

## Licence

MIT, see [LICENSE](LICENSE). MyDesktopWidget itself is a separate, closed-source application.
