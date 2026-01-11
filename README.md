# Autobar Embedded Components

This repository contains code for peripheral components used by Autobar modules. Each peripheral is a separate entity that communicates with the main PC of the module (module code can be found in [embedded](https://github.com/autobar-dev/embedded)). Communication is done over I2C so that multiple peripherals can be daisy-chained together on the same bus. Each peripheral is an STM32F103 microcontroller responsible for handling a specific function, such as button input, sensor reading or valve control. This was done in order to keep the module PC free from low-level peripheral handling code and to allow for easy addition of new peripherals in the future.

All peripherals support a heartbeat mechanism in order to monitor their status and detect failure. If the peripheral does not receive a heartbeat signal from the module PC within a certain time frame, it will assume that the connection has been lost and will reset itself.

You can find detailed documentation for each peripheral in the README files located in their respective directories.

## Button

Handles input from physical button on the module.

## Flow Meter

Handles input from flow meter sensor and calculates flow rate and total volume. Supports calibration of the sensor.

## NFC Reader

Handles writing URIs to an updatable NFC tag. When the user scans the tag with their phone, they will be directed to a webpage containing a unique identifier allowing them to initiate a module session where they're allowed to interact with the module (pour a drink).

## Valve Controller

Handles control of a solenoid valve used to dispense liquids. Supports opening and closing the valve based on commands received from the module PC.
