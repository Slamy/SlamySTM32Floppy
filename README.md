# SlamySTM32Floppy
Raw Floppy Writer using STM32F4Discovery. Supports Amiga, Atari ST, C64 Images. Even copy protected.

Software you need for this project:
* libusb-1.0.0
* Maybe Linux. This project was never tested on a Windows machine. But as libusb is multi platform it might work anyway.
* libfs-capsimage.so.4.2 (This is needed to parse IPF files if you want to read those)

Hardware you need for this project
* STM32F4Discovery. (Never tested with another controller)
* Standard 3.5" HD floppy drive for Amiga, Atari ST and Amstrad CPC
* Standard 5.25" 80 track HD floppy drive for C64 (yes it works, yes even with the small head)
* A floppy cable with both 3.5" and 5.25" connectors. The latter only if you need C64 support.
* A board you have to make for yourself to attach the STM32F4 to the Floppy Cable.
* To write a C64 flippy disk with only one index hole, a hardware change on the floppy drive is needed! PC drives only work if they get an index signal. The STM32 will produce one that you have to get to the index sensor by wire.

An OpenOCD flashing script is included to work with the integrated STLink.

This program is capable of writing certain floppy image formats.
* .adf
* .ipf (a few copy protections are written without change)
* .st
* .dsk
* .d64
* .g64 (a few copy protections are written as is. but some are not working)

I've uploaded this piece of software so that others can learn from it.
It shows communication 

Here is my blog entry on this topic:
http://slamyslab.blogspot.com/2016/04/slamy-stm32-floppy-controller-part-1.html

Disclaimer
Please keep in mind that this code was never meant to be uploaded. I can't guarantee that it's free of errors.
If this program or the circuit described here is damaging your hardware it's your fault for trying so.
The source code is provided as is.
