kodobannin
==========

コード番人 or "code keeper" is the bootloader for Hello products based on the nRF51822 chipset.

Design
======

The primary goals of kodobannin are to be small, secure and efficient. Booting the firmware and device recovery are secondary goals... ;)

Building & Programming
======================

To **build** the firmware, you will need:

1. GCC for ARM. A direct link to the current version we use is https://launchpad.net/gcc-arm-embedded/4.7/4.7-2013-q3-update/+download/gcc-arm-none-eabi-4_7-2013q3-20130916-mac.tar.bz2. There are also more recent versions (e.g. GCC 4.8) that are available for other platforms (Windows, Linux), if you'd like to be enterprising.

To **program** the firmware (upload the firmware to the device), you will need:

1. GCC for ARM (see above), and...

2. The J-Link toolchain. The direct link to the current version we use is http://www.segger.com/jlink-software.html?step=1&file=JLinkMacOSX_474. (You will need to enter in the serial number on your J-Link to download it.)

External libraries
==================

Kodobannin uses [micro-ecc](https://github.com/kmackay/micro-ecc) to provide eliptic curve cryptography support and Nordic's [SoftDevice and nRF51 SDK](https://www.nordicsemi.com/eng/Products/Bluetooth-R-low-energy/nRF51822-Development-Kit) libraries to provide a BlueTooth Low Energy stack.

Serial Debugging
================

The Band has serial port output. You will want the serial port set to 38400 8N1. (8 stop bits, no parity, 1 stop bit.) Plugging in a USB serial port should add a new device at `/dev/cu.usbserial-SOMETHING`.

Here are a few ways to get OS X to talk to serial ports. Take your pick of what you prefer. On pre-Mavericks machines, you'll need the FTDI serial driver from [FTDI](http://www.ftdichip.com/Drivers/VCP.htm) to make the serial port appear.

1. `screen /dev/cu.usbserial-* 38400`. This is the easiest option, since screen works on all Macs. Unless you know GNU screen voodoo, screen may or may not override your Terminal scrollback buffer, which you may or may not want.

2. `brew install minicom && minicom -D /dev/cu.usbserial-* -b 38400`. Minicom is a fairly old Unix program that was popular back in the warez^H^H^H^H^Hmodem days. You may want to disable the status line by pressing Esc -> O -> Screen and keyboard -> C.

3. If you are 31337 like Andre and like to use `cat`:

        exec 3<> /dev/cu.usbserial-*  # attach file descriptor 3 to serial port
        stty -f /dev/cu.usbserial-* speed 38400  # set serial port to 38400 baud
        cat <&3  # run cat with stdin attached to file descriptor 3
