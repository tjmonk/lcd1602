# lcd1602
LCD 16x2 Alphanumeric Display Interface for VarServer

## Overview

The lcd1602 service provides an interface to a 16x2 alphanumeric display
panel accessed via a PCF8574 I2C Serial to Parallel Converter.  The interface
is mapped to VarServer variables to allow simple control of the 16x2
alphanumeric display and encapsulating the complexity.

The lcd1602 module uses the following VarServer Variables

| | |
|---|---|
| Variable | Description |
| /HW/LCD1602/LINE1 | Specify the text for Line 1 of the display |
| /HW/LCD1602/LINE2 | Specify the text for Line 2 of the display |
| /HW/LCD1602/BACKLIGHT | Control the display backlight: ON (1) / OFF (0) |
| /HW/LCD1602/STATUS | Display the 16x2 LCD status |

## Command Line Arguments

The command line arguments allow customization of the lcd1602 service

| | | |
|---|---|---|
| Argument | Description | Default Value |
| -h | Display help | |
| -a | Set I2C Device address | 0x27 |
| -i | LCD Instance ID (not used) | 0 |
| -e | Enable exclusing I2C access | false |
| -v | Enable verbose output | false |

## Prerequisites

The lcd1602 service requires the following components:

- varserver : variable server ( https://github.com/tjmonk/varserver )

Of course you must have a device with a I2C interface connected to an 16x2
alphanumeric display via a PCF8574 Serial to Parallel IC to run the lcd1602
service with any meaningful results.

## Build

```
./build.sh
```

## Set up the VarServer variables

```
varserver &
mkvar -t str -n /hw/lcd1602/line1
mkvar -t str -n /hw/lcd1602/line2
mkvar -t uint16 -n /hw/lcd1602/backlight
mkvar -t str /hw/lcd1602/status

```

## Start the lcd1602 service

```
lcd1602 &
```

## Turn off the LCD backlight

```
setvar /HW/LCD1602/BACKLIGHT 0
```

## Turn on the LCD backlight

```
setvar /HW/LCD1602/BACKLIGHT 1
```

## Set the LCD screen content

```
setvar /HW/LCD1602/LINE1 "Hello World"
setvar /HW/LCD1602/LINE2 "This is a test"
```

## Query the LCD1602 status

```
getvar /HW/LCD1602/STATUS
```

```
LCD1602 Status:
Device: /dev/i2c-1
Address: 0x27
Exclusive: false
Verbose: false
Backlight: ON
Line1: Hello World
Line2: This is a test
Cursor X: 18
Cursor Y: 2
```


