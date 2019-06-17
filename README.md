# Flexit SP 30

Flexit SP 30 panel to Z-UNO.

### Hardware

- Z-UNO (https://products.z-wavealliance.org/products/1825)
- 2 x 3 V relays
- 3 optocouplers for potential free inputs
- resistors
- ISDN plug
- wires

### Build instructions:

- ISDN, pin 1: to relay 1
- ISDN, pin 3: optocoupler 1
- ISDN, pin 4: to relay 2
- ISDN, pin 5: optocoupler 2
- ISDN, pin 6: optocoupler 3
- ISDN, pin 7: GND

- Z-UNO, pin  9: to relay 1
- Z-UNO, pin 10: to relay 2
- Z-UNO, pin 11: to DS18B20 data pin
- Z-UNO, pin A1: to optocoupler 2
- Z-UNO, pin A2: to optocoupler 3
- Z-UNO, pin A3: to optocoupler 1
- Z-UNO, 7-18V: power source
- Z-UNO, GND


### Firmware

Upload to Z-UNO by:

- install the Z-UNO software version 2.1.5 for Arduino.  See '[here](https://z-uno.z-wave.me/install)' for instructions.
- upload '[firmware](https://github.com/balmli/no.almli.flexit.zuno/blob/master/Flexit_SP30_ZUNO/Flexit_SP30_ZUNO.ino)'.

