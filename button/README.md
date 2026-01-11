# Autobar Button Component

This component uses an I2C peripheral on the STM32F103 microcontroller acting as a peripheral (slave) to a module controller (master, such as a Raspberry Pi/Le Potato/BusPirate).

## I2C communication

  - Safe speed: **100kHz**
  - Peripheral address: **0x1F = 31** (*0x3F read*/*0x3E write*)

**Disclaimer:** all I2C messages have been written in the BusPirate format. 

  - `[` - start sign
  - `]` - stop sign

### Reading status

**Disclaimer:** due to my little understanding of how Arduino works with I2C, **always** request one byte from the component. Using an I2C scanner might result in the component bricking and requiring a reset.

If the button is pressed, the component will answer with either `1`/`0x31` (*pressed*) or `0`/`0x30` (*not pressed*) (always 1 byte).

```
[0x3F r]
 ^    ^
 |    |
 |    ∟ Read 1 byte
 ∟ Read address (0x3F = 0x1F << 1 + 1)
```

### Heartbeat 

Heartbeat interval is set through `#define HB_TIMEOUT ...` in `main.cpp`. The reason it's implemented is because if the I2C controller initializes after a peripheral (which can happen quite often) it might not be aware of the peripheral's existence. There is a `HardwareTimer` running at 2Hz checking whether there has been a heartbeat from the I2C controller. If there hasn't been one, the peripheral will reset itself.

To send a heartbeat message:

```
[0x7E 0x01]
 ^    ^
 |    |
 |    ∟ Heartbeat command (0x01 = heartbeat)
 ∟ Write address (0x7E = 0x3F << 1)
```

## Available commands

  - `0x01` - send heartbeat
