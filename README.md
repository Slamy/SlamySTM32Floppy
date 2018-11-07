# SlamySTM32Floppy
Raw Floppy Writer using STM32F4Discovery. Supports Amiga, Atari ST, C64 Images. Even copy protected.

You need a STM32F4Discovery board to execute this program.
An OpenOCD flashing script is included to work with the integrated STLink.

This program is capable of writing certain floppy image formats.
* .adf
* .ipf (a few copy protections are written without change)
* .st
* .dsk
* .d64
* .g64 (a few copy protections are written as is. but some are not working)

I've uploaded this piece of software so that others can learn from it.

Supported disk drives are normal 3.5" floppy drives and 5.25" floppy drives.
To write a C64 flippy disk with only one index hole a hardware change on the floppy drive is needed!

Disclaimer
Please keep in mind that this code was never meant to be uploaded. I can't guarantee that it's free of errors.
If this program or the circuit described here is damaging your hardware it's your fault for trying so.
The source code is provided as is.
