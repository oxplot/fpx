# fpx: easy USB-C power for all your devices

This repository contains the hardware design and firmware for [fpx
module](https://fpx.oxplot.com).

## Hardware

Main enabling component of fpx is
[STUSB4500](https://blog.oxplot.com/usb-pd-standalone-sink-controller/)
standalone USB-PD IC by STMicroelectronics. The rest are supporting
components and an AVR ATtiny 816 which programs the NVM flash on
STUSB4500 by converting configuration read from a light sensor.

[The KiCad files](./board) are verified for manufacturing, and include
all the part numbers.

*fpx* is a [certified open hardware](https://oshwa.org/cert) [OSHW] AU000008

## Firmware

[The firmware](./firmware) is written as a single C file with no
interrupts and provided as an Atmel Studio solution. It should however
be easily buildable by GNU toolchain. The source is well commented and
self-documenting.

## Configuration

The [main configuration tool](https://fpx.oxplot.com/#configure) exists
in the form of a web page. The Javascript part of the page documents the
encoding used to transfer configuration using light flashes.

Due to its simplicity, the configuration tool can be ported to any
device/platform that can pulse light with a reasonable timing accuracy
(e.g. Arduino).

## Licensing

All the content of this repo including the documentation, hardward design files, firmware and configuration utilities are licensed under the [Revised BSD license](./LICENSE).

## History

[See my blog post](https://blog.oxplot.com/fpx) about fpx and its
predecessor
[fabpide2](https://blog.oxplot.com/usb-pd-standalone-sink-controller/).
