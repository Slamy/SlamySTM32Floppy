#include "floppy_crc.h"

unsigned short crc=0xFFFF;
unsigned int crcShiftedBytes=0;
unsigned char crcCheckedBytes[1024];


void crc_shiftByte(unsigned char b)
{
	if (crcShiftedBytes < sizeof (crcCheckedBytes))
		crcCheckedBytes[crcShiftedBytes]=b;
	crcShiftedBytes++;

	int i;
	for (i = 0; i < 8; i++)
		crc = (crc << 1) ^ ((((crc >> 8) ^ (b << i)) & 0x0080) ? 0x1021 : 0);

	//printf("crc_shiftByte %x -> %x\n",b,crc);
}

void crc_printCheckedBytes()
{
	int i;
	for (i=0; i< crcShiftedBytes; i++)
	{
		printf("%02x ",crcCheckedBytes[i]);
		if ((i%16)==15)
			printf("\n");
	}
	printf("\n");
}


