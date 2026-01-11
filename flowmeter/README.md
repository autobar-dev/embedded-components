# Autobar Flow Meter Component

This component uses an I2C peripheral on the STM32F103 microcontroller acting as a peripheral (slave) to a module controller (master, such as a Raspberry Pi/Le Potato/BusPirate).

It uses EEPROM to store what volume per pulse is. In case it is not set, it will default to `170` per pulse.

## I2C communication

  - Safe speed: **100kHz**
  - Peripheral address: **0x2F = 47** (*0x5F read*/*0x5E write*)

**Disclaimer:** all I2C messages have been written in the BusPirate format. 

  - `[` - start sign
  - `]` - stop sign

### Reading volume

**Disclaimer:** due to my little understanding of how Arduino works with I2C, **always** request 4 bytes from the component. Using an I2C scanner might result in the component bricking and requiring a reset.

When reading the component will answer with the current volume as an *unsigned 32-bit integer* in 4 bytes. The volume is in **microleters** to make calculations faster on the microcontroller side.

```
[0x5F r:4]
 ^    ^
 |    |
 |    ∟ Read 4 bytes
 ∟ Read address (0x5F = 0x2F << 1 + 1)
```

Example output:

`0x00 0x07 0xC2 0x36` → `0x0007C236` → 508,470 microleters

### Resetting the counter

To reset the counter, send the following message:

```
[0x5E 0x02]
 ^    ^
 |    |
 |    ∟ Reset command
 ∟ Write address (0x5E = 0x2F << 1)
```

### Setting volume per pulse

To set the volume per pulse, send the following message:

```
[0x5E 0x03 0x00 0x00 0x00 0x96]
 ^    ^    ^
 |    |    |
 |    |    ∟ Volume per pulse (unsigned 32-bit integer), e.g. 0x00000096 = 150
 |    ∟ Set volume per pulse command
 ∟ Write address (0x5E = 0x2F << 1)
```

This value must be **4 bytes long**.

### Entering calibration mode

Entering calibration mode will clear current total volume and count pulses until calibration is finished.

```
[0x5E 0x04]
 ^    ^
 |    |
 |    ∟ Enter calibration mode command
 ∟ Write address (0x5E = 0x2F << 1)
```

### Finishing calibration

Finishing calibration will set volume per pulse to provided volume divided by the number of pulses counted and save this value to EEPROM.

```
[0x5E 0x05 0x00 0x07 0xA1 0x20]
 ^    ^    ^
 |    |    |
 |    |    ∟ Total volume (unsigned 32-bit integer), e.g. 0x0007A120 = 500,000
 |    ∟ Finish calibration command
 ∟ Write address (0x5E = 0x2F << 1)
```

This value must be **4 bytes long**.

### Cancelling calibration

Cancelling calibration will reset the counter and exit calibration mode.

```
[0x5E 0x06]
 ^    ^
 |    |
 |    ∟ Cancel calibration command
 ∟ Write address (0x5E = 0x2F << 1)
```

### Heartbeat 

Heartbeat interval is set through `#define HB_TIMEOUT ...` in `main.cpp`. The reason it's implemented is because if the I2C controller initializes after a peripheral (which can happen quite often) it might not be aware of the peripheral's existence. There is a `HardwareTimer` running at 2Hz checking whether there has been a heartbeat from the I2C controller. If there hasn't been one, the peripheral will reset itself.

To send a heartbeat message:

```
[0x5E 0x01]
 ^    ^
 |    |
 |    ∟ Heartbeat command (0x01 = heartbeat)
 ∟ Write address (0x5E = 0x2F << 1)
```

## Available commands

The following is a list of all available commands (could be expanded in the future):

  - `0x01` - send heartbeat
  - `0x02` - reset total volume
  - `0x03` - set volume per pulse to a specific value
  - `0x04` - enter calibration mode
  - `0x05` - finish calibration
  - `0x06` - cancel calibration