# Board profiles

A board profile is a JSON file that describes an ESP32 board and its screen: the chip, the display
driver, the resolution, the pins, the touch controller and the LED. The ready firmware (coming later)
reads it at startup, so one firmware serves every profile and nobody recompiles for a pin change.
Until then, a profile is also the reference for setting up a display library by hand.

Every profile is checked against [`board-profile.schema.json`](board-profile.schema.json).

| Profile | Board | Status |
|---|---|---|
| [`cyd-2432s028r`](cyd-2432s028r.json) | Cheap Yellow Display (ESP32-2432S028R), ILI9341 panel | **Official** - tested on real hardware |
| [`cyd-2432s028r-st7789`](cyd-2432s028r-st7789.json) | Cheap Yellow Display (ESP32-2432S028R), ST7789 panel | Community - not yet tested by the maintainers |

**Official** means tested on real hardware by the maintainers. Community profiles are welcome as pull
requests and are supported by their authors.

## The Cheap Yellow Display has two panels

Boards sold as ESP32-2432S028R carry one of two different display chips, **and you cannot tell which
from the outside** - not even from the number of USB ports. If the screen stays white, or shows
noise, you have the other panel: switch to the other setup below.

### TFT_eSPI setup, ILI9341 panel (`cyd-2432s028r`)

For PlatformIO, in `platformio.ini`:

```ini
lib_deps =
  MyDesktopWidget
  bodmer/TFT_eSPI@^2.5.43
build_flags =
  -DUSER_SETUP_LOADED=1
  -DILI9341_2_DRIVER=1      ; the "_2" matters: the plain ILI9341 driver shows noise on this panel
  -DTFT_WIDTH=240 -DTFT_HEIGHT=320
  -DTFT_MISO=12 -DTFT_MOSI=13 -DTFT_SCLK=14 -DTFT_CS=15 -DTFT_DC=2 -DTFT_RST=-1
  -DTFT_BL=21 -DTFT_BACKLIGHT_ON=HIGH
  -DUSE_HSPI_PORT=1 -DSPI_FREQUENCY=40000000
  -DLOAD_GLCD=1 -DLOAD_FONT2=1 -DLOAD_FONT4=1
```

For the Arduino IDE, put the same settings in TFT_eSPI's `User_Setup.h` as `#define` lines
(`#define ILI9341_2_DRIVER`, `#define TFT_MISO 12`, and so on).

### TFT_eSPI setup, ST7789 panel (`cyd-2432s028r-st7789`)

As above, with these three lines changed:

```ini
  -DST7789_DRIVER=1         ; instead of ILI9341_2_DRIVER
  -DTFT_RGB_ORDER=TFT_BGR
  -DSPI_FREQUENCY=55000000
```
