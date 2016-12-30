LDLIBS=-lusb-1.0 -l:libfs-capsimage.so.4.2 -lm
CFLAGS=-g -Wall

slamySTM32Floppy: amiga_ipf.c  amstradCpc_dsk.c  c64_d64_g64.c  noHeaderImage_st_adf.c  slamySTM32Floppy.c

install: slamySTM32Floppy
	install slamySTM32Floppy /usr/local/bin

