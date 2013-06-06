#!/bin/bash

scons
avrdude -c usbasp \
  -C /Applications/Arduino.app/Contents/Resources/Java/hardware/tools/avr/etc/avrdude.conf \
  -p t85 \
  -U lfuse:w:0xc1:m \
  -U hfuse:w:0xd7:m \
  -U efuse:w:0xff:m \
  -U flash:w:TinyFiveISR.hex:i

# bit low        high
# 7   [ ] CKDIV8 [ ] RSTDISBL
# 6   [ ] CKOUT  [ ] DWEN
# 5   [X] SUT1   [X] SPIEN
# 4   [X] SUT0   [ ] WDTON
# 3   [X] CKSEL3 [ ] EESAVE
# 2   [X] CKSEL2 [ ] BODLEVEL2
# 1   [X] CKSEL1 [ ] BODLEVEL1
# 0   [ ] CKSEL0 [ ] BODLEVEL0

# upshot is: run at 16mhz PLL clock, enable SPI programming, disable brown-out
# detection.