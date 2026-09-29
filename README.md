# MyDesktopWidget Screens

Show live PC stats from [MyDesktopWidget](https://mydesktopwidget.com) on an ESP32 screen: CPU, GPU,
temperatures, network and media.

> **Early days.** This repository is being set up. The library, the firmware and the protocol
> specification arrive with the first release; until then nothing here is usable.

## How it works

MyDesktopWidget runs on your Windows PC and reads its sensors. Plug an ESP32 with a screen into the
PC over USB, and the engine sends it the readings your sketch asks for. What the screen does with them
is up to you.

There are three ways to use it, and you can move between them without changing your hardware:

| Level | For | You get |
|---|---|---|
| **1. Data** | Anyone who wants full control | An Arduino / PlatformIO library. Subscribe to sensors and draw them however you like: LVGL, TFT_eSPI, LEDs, a servo, anything |
| **2. Designed screens** | Makers who prefer a visual designer | Design an LVGL screen in SquareLine Studio or EEZ Studio, name its widgets after sensors, and they update live |
| **3. Plug and play** | Beginners | Flash the ready firmware from the website and pick a widget skin in MyDesktopWidget. No code |

## What is in this repository

| Path | What |
|---|---|
| `src/` | The `MyDesktopWidget` Arduino / PlatformIO library |
| `examples/` | Example sketches (coming with the first release) |
| `extras/boards/` | Board profiles: a JSON description of a board and its screen |
| `extras/firmware/` | The ready firmware for plug-and-play use |

## Supported hardware

**Officially tested:** the "Cheap Yellow Display", ESP32-2432S028R (2.8" 320×240 with touch).

Other ESP32 boards and screens can be described with a board profile. Community profiles are welcome
as pull requests; they are supported by their authors.

## Requirements

- Windows with [MyDesktopWidget](https://mydesktopwidget.com) installed. Levels 1 and 2 work with the
  free version.
- An ESP32 connected by USB.

## Licence

MIT, see [LICENSE](LICENSE). MyDesktopWidget itself is a separate, closed-source application.
