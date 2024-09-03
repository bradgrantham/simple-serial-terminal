![Build](https://github.com/bradgrantham/simple-serial-terminal/actions/workflows/c-cpp.yml/badge.svg)

# Introduction

This tool opens a device at a particular baud rate and "8,N,1", hooks the terminal to the device, and gets out of the way.

Run it something like the following, then press tilde (`~`) then period to exit.

```
serial /dev/tty01 19200
```

I don't like serial communications programs taking over the terminal.  So I use this little program instead.

Use cases include:
* Connecting to UARTs of microcontrollers like the Raspberry Pi Pico, STM32 series, and PIC series.
* Some smartphones have a serial console bootloader mode in which you can update the firmware, debug flash memory, and things like that.  For example, one can power on some phones by HTC while pressing the phone's camera button and the phone will automatically export a USB serial device on the mini-USB port.
* GRUB has the ability to use a serial port as its terminal and `serial` from another machine can allow a user to drive GRUB.
* Communicating with the consoles of servers (e.g. old SGI IRIX boxes 20 in the early 2000's).

# Features

* `serial` contains very rudimentary support for macros to aid repeatedly issuing very similar, very arcane bootloader commands.
* `serial` is very easy to build and very easy to run.  (Once you know the name of the serial device!)

# Building

Build it on Linux, MacOS,and Cygwin e.g. from bash or zsh using:

```
    CXXFLAGS="--std=c++17 -Wall" make serial
```

Run `serial` without arguments to see usage information.

Please create issues if you find a bug or create a pull request with a fix or enhancement.

(One thing `serial` really needs is the ability to change the escape character from tilde to something else.  I find myself logging out of `ssh` more often than I'd like when I really meant to just disconnect from the serial port.)

## Updates

### May 9th, 2010

Joost Bruynooghe pointed out that I was missing an argument in a printf on line 143, and that could conceivably have lead to junk output or possibly a crash.  The code was **not** compiling without warnings. I have updated the source code with his fix.  Thanks, Joost!

### June 18th, 2022

Chris Browning mentioned `serial` in a [StackOverflow post](https://stackoverflow.com/a/43925904) many years ago.  He posted his version with a bug fix at https://github.com/lime45/serial .

### January 6th, 2024

Nick Thomas (nethomas1968@gmail.com) tested this in WSL2 (Windows Subsystem for Linux).

To use `serial`, you will need to install the Windows tool `usbipd-win` to share USB devices with the Linux subsystem. Nick used version 4.0.0 from https://github.com/dorssel/usbipd-win .

The following example uses Windows PowerShell as administrator.

1) See what ports Windows has available:
```
 C:\> usbipd list
```

2) Make the port shareable.  For example, to make Id "5-3" available to the Linux subsystem:
```
 C:\> usbipd bind -b 5-3
```

3) Share that port:
```
 C:\> usbipd attach --wsl --busid 5-3
```

In the Linux terminal, look for /dev/ttyUSB0.  That can be used as the port for `serial`, e.g.:
```
$ serial /dev/ttyUSB0 19200
```

### September 2nd, 2024

* Fixed a long-standing bug in which 2 stop bits were being requested because of an incorrect flag setting.
* Upgraded the style a tiny bit.
* Handle ENXIO, which at least MacOS uses to indicate that the device was disconnected.
