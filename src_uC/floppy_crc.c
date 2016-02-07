#include "floppy_crc.h"

unsigned short crc=0xFFFF;

void crc_shiftByte(unsigned char b)
{
	int i;
	for (i = 0; i < 8; i++)
		crc = (crc << 1) ^ ((((crc >> 8) ^ (b << i)) & 0x0080) ? 0x1021 : 0);
}
