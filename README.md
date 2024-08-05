# Recover from readonly NOR flash issue -> get write access again

**Author**: Stephan Enderlein \
**License**: BSD-3-Clause

The Finder OPTA devices NOR flash chip will become readonly
somethimes. The reason is not known and must be a bug in Arduino
drivers or mbed-os.

The external flash (build in the device and externally connected to the MCU STM32H7) is used as a block device carrying partitions and
filesystems.

When this NOR flash is protected, probably hardware protection is
enabled without any reason. Then the file system can only be read.

Any attempt to write, reformat, partitioning will not return an error. It is simply not working and ignores any write command to
memory or registers of the NOR chip. This is how the flash chip works.


**Howto disable flash protection and Background**:

Recovering your OPTA is only possible when you disable HW protection. \
QSPI has several "speed-modes" using either only 2 data lines, 4 or 8.\
Two of those pins are used for either transfering data or as HW protection PIN.\
This depends on the speed mode.

The Opta device has connected all needed data lines to STM32H7 mcu.\
Somehow Arduino or Driver code does set the important flag accidentally.
This could be uninialized bits in variables that are then random.

If at some point later the HW Pin gets low the flash hardware protection gets
activated permantently.

This HW protection can only be reset when you have access to the HW protection pin.\
There is no schematics available or any test point on the board, that allows to pull this line to high.\
But there is an easy way todo it.

**The trick**:

As I know that all data lines are connected to MCU (stm32h7) and this MCU allows to
move internal functions to different pins. Function are something like I2C,USB,UARTs,PWMs.

**AND ALSO QSPI**.

With this in mind, after you have initialized QSPI I redefine the data pin which
also controls hardware protection as an normal GPIO input pin. \
This is possible, because I only use QSPI commands that are available for QSPI_CFG_BUS_SINGLE (speed mode).


**Result**: The GPIO pin gets into high-z state (input). The QSPI flash has internally a pull up resistor. This pin is now high and disables the HW flash protection.

The next steps are easy:
  - enable writing to flash (WEL bit)
  - reset all protection bits.

~~~cpp
pinMode(QSPI_SO2, INPUT);
~~~


I have added the simple program. If you need it as 'sketch', which is just a renamed c/c++ file without the include for "Arduino.h", rename and import it to Arduino-IDE.

In your software project you can read the status at some point and
reset the flash protection when it happend.

But don't forget to reboot or reinitalize QSPI when you
use a different value as QSPI_CFG_BUS_SINGLE for fast communication.
