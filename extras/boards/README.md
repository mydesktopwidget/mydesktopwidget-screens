# Board profiles

A board profile is a JSON file that describes an ESP32 board and its screen: the chip, the display
driver, the resolution, the pins, the touch controller and any buttons. The ready firmware reads it at
startup, so one firmware serves every profile and nobody recompiles for a pin change.

The schema is being written. The first official profile is the Cheap Yellow Display
(ESP32-2432S028R).

Community profiles are welcome as pull requests. A profile is **official** only when it has been
tested on real hardware by the maintainers; others are labelled community profiles and are supported
by their authors.
