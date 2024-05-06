/*
Copyright (c) 2024, Stephan Enderlein. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

License: BSD-3-Clause
*/


#include <stdint.h>
#include <Arduino.h>
#include <QSPI.h>

// this macro device two function that are called by mbed::os to redirect stdout (which printf uses)
// to serial "stream"
// Allowing using printf.
#include <macros.h>
REDIRECT_STDOUT_TO(Serial);


// devine QSPI pins. MBED_CONF_QSPIF_QSPI_Ixxx are defined by selected device (e.g. OPTA)
PinName io0 = MBED_CONF_QSPIF_QSPI_IO0;
PinName io1 = MBED_CONF_QSPIF_QSPI_IO1;
PinName io2 = MBED_CONF_QSPIF_QSPI_IO2;
PinName io3 = MBED_CONF_QSPIF_QSPI_IO3;
PinName sclk = MBED_CONF_QSPIF_QSPI_SCK;
PinName csel = MBED_CONF_QSPIF_QSPI_CSN;
int clock_mode = MBED_CONF_QSPIF_QSPI_POLARITY_MODE;
int freq = MBED_CONF_QSPIF_QSPI_FREQ;

mbed::QSPI qspi(io0, io1, io2, io3, sclk, csel, clock_mode);
qspi_status_t status;


// some helper
static void _writeEnable();
static bool _waitForReady();

// waits that QSPI flash is free to use (no other communiction active)
bool _waitForReady()
{
    int retries = 10000; //10s

    do {
        retries--;
        char buf[1];

        // read status
        status = qspi.command_transfer(0x05, -1, nullptr, 0, &buf[0], 1);
        if (status)
        {
            printf("ERROR: command_transfer() status: %d\n", status);
            return false;
        }

        if(!(buf[0] & 0x01))
        {
          return true;
        }

        delay(1);

    } while ( retries);

    return false;
}

// enable writing to flash (to flash or to registers)
void _writeEnable()
{
    char buf[1];

    printf("_writeEnable()\n");

    // flash must be free to use
    if(!_waitForReady())
    {
      printf("ERROR: not ready (WIP==1)\n");
      return;
    }

    // first enable write to status register. this sets the WEL bit in status register
    status = qspi.command_transfer(0x06, -1, nullptr, 0, nullptr, 0);
    if (status)
    {
        printf("command_transfer: %d\n", status);
    }

    // wait until transfer has finished
    if(!_waitForReady())
    {
      printf("ERROR: not ready (WIP==1)\n");
      return;
    }

    //check WEL bit
    status = qspi.command_transfer(0x05, -1, nullptr, 0, buf, 1);
    if (status)
    {
        printf("ERROR: command_transfer() status: %d\n", status);
        return;
    }
}

bool configureQSPI()
{
    // tell stm32h7 qspi interface how to talk to qspi flash
    // This depends on the flash chip (see chip documentation)
    status = qspi.configure_format(
      QSPI_CFG_BUS_SINGLE, QSPI_CFG_BUS_SINGLE,
      QSPI_CFG_ADDR_SIZE_24,
      QSPI_CFG_BUS_SINGLE,
      QSPI_CFG_ADDR_SIZE_8,
      QSPI_CFG_BUS_SINGLE, 0);

    if (status)
    {
        printf("ERROR: configure_format() status: %d\n", status);
        return false;
    }

    status = qspi.set_frequency(freq);
    if (status)
    {
        printf("ERROR: set_frequency() status: %d\n", status);
        return false;
    }

    return true;
}

void readStatusRegister()
{
    printf("readStatusRegister()\n");

    char buf[3];
    status = qspi.command_transfer(0x05, -1, nullptr, 0, &buf[0], 1);
    if (status)
    {
        printf("ERROR: command_transfer() status: %d\n", status);
        return;
    }

    // this register shows the current flash protection.
    // SRP0 shows if it is possible to change those flags
    // if you have set some of the * bits then flash protection is turned on.
    // Note, that if hardware protection of the flash is active, then you can not write to this
    // register to clear the protection.
    printf("Status Register 1: 0x%02x\n", buf[0]);
    printf("    Write in Progress  (WIP): %d\n", (buf[0] & 0x01) ? 1 : 0);
    printf("    Write Enable Latch (WEL): %d\n", (buf[0] & 0x02) ? 1 : 0);
    printf("    *BP0: %d\n", (buf[0] & 0x04) ? 1 : 0);
    printf("    *BP1: %d\n", (buf[0] & 0x08) ? 1 : 0);
    printf("    *BP2: %d\n", (buf[0] & 0x10) ? 1 : 0);
    printf("    *BP3: %d\n", (buf[0] & 0x20) ? 1 : 0);
    printf("    *BP4: %d\n", (buf[0] & 0x40) ? 1 : 0);
    printf("    *Status Register Protect (SRP0): %d\n", (buf[0] & 0x80) ? 1 : 0);

    status = qspi.command_transfer(0x35, -1, nullptr, 0, &buf[1], 1);
    if (status)
    {
        printf("ERROR: command_transfer() status: %d\n", status);
        return;
    }
    printf("Status Register 2: 0x%02x\n", buf[1]);
    printf("    *Status Register Protect (SRP1): %d\n", (buf[1] & 0x01) ? 1 : 0);
    printf("    *Quad Enable (QE): %d\n", (buf[1] & 0x02) ? 1 : 0);
    printf("    SUS2: %d\n", (buf[1] & 0x04) ? 1 : 0);
    printf("    LB1: %d\n", (buf[1] & 0x08) ? 1 : 0);
    printf("    LB2: %d\n", (buf[1] & 0x10) ? 1 : 0);
    printf("    LB3: %d\n", (buf[1] & 0x20) ? 1 : 0);
    printf("    CMP: %d\n", (buf[1] & 0x40) ? 1 : 0);
    printf("    SUS2: %d\n", (buf[1] & 0x80) ? 1 : 0);

    status = qspi.command_transfer(0x15, -1, nullptr, 0, &buf[2], 1);
    if (status)
    {
        printf("ERROR: command_transfer() status: %d\n", status);
        return;
    }
    printf("Status Register 3: 0x%02x\n", buf[2]);
    printf("    DRV0: %d\n", (buf[2] & 0x20) ? 1 : 0);
    printf("    DRV1: %d\n", (buf[2] & 0x40) ? 1 : 0);

    printf("\n");

}

// only possible if no HW protection is truned on.
// in case of Finder OPTA, this happens accedentially and is a bug
// within their driver or mbed::OS port/libs.
// you have to disable HW protection first. (which is possible)
void resetFlashProtection()
{
    printf("resetFlashProtection()\n");

    if(!_waitForReady())
    {
      printf("ERROR: not ready (WIP==1)\n");
      return;
    }

    _writeEnable();

    char reg = 0; // 0x88; SRP0=1 und BP1
    status = qspi.command_transfer( 0x01, -1, &reg, 1, nullptr, 0);
    if (status)
    {
        printf("command_transfer: %d\n", status);
    }

    // wait until finished; else status register shows invalid values for other bits
    if(!_waitForReady())
    {
      printf("ERROR: not ready (WIP==1)\n");
      return;
    }

}


void setup()
{
    Serial.begin(115200);
    while (!Serial);
    printf("start\n");

    configureQSPI();

    // Recover-Trick:
    // Recovering your OPTA is only possible when you disable HW protection.
    // Background: QSPI has several "speed-modes" using either only 2 data lines, 4 or 8.
    // Two of those pins are used for either transfering data or as HW protection PIN.
    // This depends on the speed mode.
    //
    // The Opta device has connected all needed data lines to STM32H7 mcu.
    // Somehow Arduino or Driver code does set the important flag accedentially. If then
    // the HW Pin gets low (somehow) the flash hardware protection gets activated permantently.
    // This HW protection can only be reset when you have access to the HW protection pin.
    //
    // There is no scematics or any test point on the board, that allows to pull this line high.
    // But there is a much simpler way todo it.
    //
    // The trick:
    //   As I know that all data lines are connected to MCU (stm32h7) and this MCU allows to
    //   move internal functions to different pins. Function are something like I2C,USB,UARTs,PWMs
    //   AND ALSO QSPI.
    //   With this in background, after you have initialized QSPI I redefine the data pin which
    //   also controls hardware protection as an normal GPIO input pin.
    //   This is possible, because I only use QSPI commands that are available for QSPI_CFG_BUS_SINGLE.
    //
    // Result: the CPIO pin gets into high-z state (input). The QSPI flash has internally a pull up
    //         resistor which disables the HW flash protection.
    //
    // The next steps are easy:
    //   - enable writing to flash (WEL bit)
    //   - reset all protection bits.

    pinMode(QSPI_SO2, INPUT);

    //print current status register+check if WEL can be set
    printf("---- current status ----\n");
    readStatusRegister();

    // tell qspi that I want to write data. nor flash needs this to avoid accedential writes to
    // registers or memory during booting a device where signals on data/control lines are not determined.
    printf("---- write enable + checking WEL bit ----\n");
    _writeEnable();
    readStatusRegister();

    //reset flash protection
    printf("---- reset flash protection ----\n");
    resetFlashProtection();

    //verify
    printf("---- verify status register ----\n");
    readStatusRegister();

    printf("---end---\n");
}

void loop()
{
    // Your main loop code (if any) can go here
}
