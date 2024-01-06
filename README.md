This tool opens
a device at a particular baud rate, hooks the terminal to the serial
port, and gets out of the way.  Run it something like the following,
then press tilde (`~`) then period to exit.

```
serial /dev/tty01 19200
```

I used to use `minicom` from Linux as a serial port terminal, but I found
I didn't like `minicom` taking over the terminal.  So I found at the time
I was using this junky little program instead of `minicom.`

Use cases include:
* Communicating with the consoles of servers (e.g. old SGI IRIX boxes 20 in the early 2000's)
* Some smartphones have a serial console bootloader mode in which you can update the firmware, debug flash memory, and things like that.  For example, one can power on some phones by HTC while pressing the phone's camera button and the phone will automatically export a USB serial device on the mini-USB port.
* `serial` contains **very** rudimentary support for macros to aid repeatedly issuing very similar, very arcane bootloader commands.
* GRUB has the ability to use a serial port as its terminal and `serial` from another machine can allow a user to drive GRUB

Build it on Linux, MacOS,and Cygwin using e.g. from bash or zsh using:
```
    CFLAGS="--std=c++11 -Wall" make serial
```

Run `serial` without arguments to see usage information.

Please create issues if you find a bug or create a pull request with a fix or enhancement.

(One thing `serial` really needs is the ability to change the escape character
from tilde to something else.  I find myself logging out of `ssh` more
often than I'd like when I really meant to just disconnect from the
serial port.)

*May 9th, 2010*

Joost Bruynooghe pointed out that I was missing an argument in a printf
on line 143, and that could conceivably have lead to junk output or
possibly a crash.  The code was **not** compiling without warnings.
I have updated the source code with his fix.  Thanks, Joost!

*June 18th, 2022*

Chris Browning mentioned `serial` in a
[StackOverflow post](https://stackoverflow.com/a/43925904)
many years ago.  He posted his version with a bug fix at
https://github.com/lime45/serial .

*January 6th, 2024*

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
