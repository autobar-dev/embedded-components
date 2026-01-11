# Autobar Embedded Valve Component

This component uses an I2C peripheral on the STM32F103 microcontroller acting as a peripheral (slave) to a module controller (master, such as a Raspberry Pi/Le Potato/BusPirate).

It uses a MOSFET transistor to turn a valve on and off.

## I2C communication

  - Safe speed: **100kHz**
  - Peripheral address: **0x3F = 63** (*0x7F read*/*0x7E write*)

**Disclaimer:** all I2C messages have been written in the BusPirate format. 

  - `[` - start sign
  - `]` - stop sign

### Turning valve on 

To turn the valve on, send the following message:

```
[0x7E 0x02]
 ^    ^
 |    |
 |    ∟ Turn valve on command
 ∟ Write address (0x7E = 0x3F << 1)
```

The active led (blue) will light up too.

### Turning valve off

To turn the valve off, send the following message:

```
[0x7E 0x03]
 ^    ^
 |    |
 |    ∟ Turn valve off command
 ∟ Write address (0x7E = 0x3F << 1)
```

The active led (blue) will turn off as well.

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

The following is a list of all available commands (could be expanded in the future):
  
  - `0x01` - send heartbeat 
  - `0x02` - turn valve on
  - `0x03` - turn valve off
