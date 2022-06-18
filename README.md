Many years ago I cobbled together something together to open a serial port for communicating with the consoles of some SGI boxes.  It just opened a device at a particular baud rate, hooked my terminal to the serial port, and got out of the way.  Ran it something like the following, then pressing tilde then period to exit.

```
serial /dev/tty01 19200
```

A little later I found myself working with embedded devices, essentially prototype phones.   Many modern smartphones have a serial console bootloader mode in which you can update the firmware, debug flash memory, and things like that.  You can bring up at least some phones by HTC, for example, while pressing the phone's camera button, and it will automatically export a USB serial device on the mini-USB port.  So I rigged the `serial` program to run under Linux and added **very** rudimentary support for macros.  It helped greatly to have macros since I was repeatedly issuing very similar, very arcane bootloader commands.

I used to use `minicom` from Linux as a serial port terminal, but I found it was too sophisticated for my regular use.  `Minicom` has a deep menu system for configuring everything about the serial port from stop bits to send-expect dialing scripts.  Most annoyingly, though, it  takes over the screen.  So I found I was using this junky little program instead of `minicom.`

At AMD, I found myself multibooting often between variants of Windows and Linux operating systems.  When I was in the office, it was easy to lean over to the machine in question and select the right entry from GRUB, the Gnu boot loader.  But I worked at home sometimes and needed to reboot the machine into a different operating system from time to time.  GRUB has the ability to use a serial port as its terminal. So I had hooked the test machine to my desktop Linux workstation.  On the Linux workstation, I ran `serial` to connect to GRUB.

Here's the [source code](http://plunk.org/~grantham/serial.cpp) to this beast, just in case someone might find it helpful.  I borrowed some code from the Internet for the basic serial port control, but now I can't find the original.  So I'd be happy to provide a reference if someone ever finds it.

I can think of several things that could be improved or fixed.  Submit a pull request.  As one example, it probably would be cleaner if it used STL vectors and maps.  As another, it has a nearly-400-line `main()` , which we all know is sacrilege.  But it's an essential workhorse for me now, any time I have to interface to a serial port.  I haven't provided a Makefile for it since it seems to build on Linux, MacOS, and Cygwin without warnings just using `"CFLAGS=-Wall make serial"` and the default `make` settings.  Run `serial` without arguments to see a basic help message.

(One thing it really needs is the ability to change the escape character from tilde to something else.  I find myself logging out of `ssh` more often than I'd like when I really meant to just disconnect from the serial port.)

*May 9th, 2010*

Joost Bruynooghe pointed out that I was missing an argument in a printf on line 143, and that could conceivably have lead to junk output or possibly a crash.  The code was **not** compiling without warnings.  I have updated the source code with his fix.  Thanks, Joost!