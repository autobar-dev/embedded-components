# Autobar NFC Component

This component uses both I2C peripherals on the STM32F103 microcontroller acting as a peripheral (slave) to a module controller (master, such as a Raspberry Pi/Le Potato/BusPirate) and a controller (master) to an ST25DV16 dynamic NFC tag module plugged into the board.

The code and board has been designed in such a fashion, that the user doesn't have to worry about the Controller-Peripheral communication between the microcontroller and the NFC module.

## I2C communication

  - Safe speed: **100kHz**
  - Peripheral address: **0x0F = 15** (*0x1F read*/*0x1E write*)

**Disclaimer:** all I2C messages have been written in the BusPirate format. 

  - `[` - start sign
  - `]` - stop sign

### Writing new URIs

The I2C message sent to the component should look as follows:

```
[0x1E 0x02 0x01 0x61 0x62 0x63 0x2E 0x63 0x6F 0x6D]
 ^    ^    ^    ^
 |    |    |    |
 |    |    |    ∟ URI (here abc.com)
 |    |    ∟ URI protocol (full supported list below)
 |    ∟ Write command (0x02 = write new URI)
 ∟ Write address (0x1E = 0x0F << 1)
```

The above message will write `http://www.abc.com` to the NFC tag.

### Reading status

**Disclaimer:** due to my little understanding of how Arduino works with I2C, **always** request one byte from the component. Using an I2C scanner might result in the component bricking and requiring a reset.

If the new URI has been written correctly, the component will answer with either `K` (*OK*) or `E` (*ERROR*) (always 1 byte). After reading, the response will become `\0`.

```
[0x1F r]
 ^    ^
 |    |
 |    ∟ Read 1 byte
 ∟ Read address (0x1F = 0x0F << 1 + 1)
```

### Heartbeat 

Heartbeat interval is set through `#define HB_TIMEOUT ...` in `main.cpp`. The reason it's implemented is because if the I2C controller initializes after a peripheral (which can happen quite often) it might not be aware of the peripheral's existence. There is a `HardwareTimer` running at 2Hz checking whether there has been a heartbeat from the I2C controller. If there hasn't been one, the peripheral will reset itself.

To send a heartbeat message:

```
[0x1E 0x01]
 ^    ^
 |    |
 |    ∟ Heartbeat command (0x01 = heartbeat)
 ∟ Write address (0x1E = 0x0F << 1)
```

## Available commands

The following is a list of all available commands (could be expanded in the future):
  
  - `0x01` - send heartbeat 
  - `0x02` - write new URI

## Supported protocols

  - `0x01` - `http://www.`
  - `0x02` - `https://www.`
  - `0x03` - `http://`
  - `0x04` - `https://`
  - `0x05` - `tel:`
  - `0x06` - `mailto:`
  - `0x07` - `ftp://anonymous:anonymous@`
  - `0x08` - `ftp://ftp.`
  - `0x09` - `ftps://`
  - `0x0A` - `sftp://`
  - `0x0B` - `smb://`
  - `0x0C` - `nfs://`
  - `0x0D` - `ftp://`
  - `0x0E` - `dav://`
  - `0x0F` - `news:`
  - `0x10` - `telnet://`
  - `0x11` - `imap:`
  - `0x12` - `rtsp://`
  - `0x13` - `urn:`
  - `0x14` - `pop:`
  - `0x15` - `sip:`
  - `0x16` - `sips:`
  - `0x17` - `tftp:`
  - `0x18` - `btspp://`
  - `0x19` - `btl2cap://`
  - `0x1A` - `btgoep://`
  - `0x1B` - `tcpobex://`
  - `0x1C` - `irdaobex://`
  - `0x1D` - `file://`
  - `0x1E` - `urn:epc:id:`
  - `0x1F` - `urn:epc:tag:`
  - `0x20` - `urn:epc:pat:`
  - `0x21` - `urn:epc:raw:`
  - `0x22` - `urn:epc:`
  - `0x23` - `urn:nfc:`
