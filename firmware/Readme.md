Firmware for fpx is written in C in the standard environment provided by
Atmel via their Atmel Studio IDE on Windows. It should be possible to
both compile and program this firmware using free and open source
software such as GNU toolchain and avrdude on linux and other platforms.

The firmware code is written as a single C file with no external
dependencies which should make porting it easier.
