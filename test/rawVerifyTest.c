#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

unsigned char cylinderBuffer1[]={0xff, 0xff, 0xff, 0xff, 0xc2, 0x12, 0x23 ,0xff, 0x88};
unsigned char cylinderBuffer2[]={0xff, 0xff, 0xff, 0xff, 0x02, 0x12, 0x23 ,0xff, 0x88};
unsigned char *cylinderBuffer;
int cylinderSize;


void printCharBin(unsigned char val)
{
	int i;

	for (i=0;i<8;i++)
	{
		if (val&0x80)
		{
			printf("1");
		}
		else
		{
			printf("0");
		}

		val<<=1;
	}
}


uint8_t *verifySectorData;
int verifySectorDataBytesLeft=0;
int verifySectorDataShift=0;
int verifySectorDataNextByteMask=0;

int floppy_raw_find1541Sync()
{
	verifySectorData=(uint8_t*)cylinderBuffer;
	verifySectorDataBytesLeft=cylinderSize-2;

	//wir suchen nach dem Muster * 11111 11111 0
	int i,j;
	uint32_t wordAccu=0;
	uint32_t byteAccu=0;

	for (i=0; i < cylinderSize; i++)
	{
		byteAccu = *verifySectorData;
		for (j=0; j < 8; j++)
		{
			wordAccu<<=1;

			if (byteAccu & 0x80)
				wordAccu|=1;

			byteAccu<<=1;

			if ((wordAccu & 0b11111111111)==0b11111111110)
			{

				verifySectorDataShift=j;
				verifySectorDataNextByteMask=0xff >> verifySectorDataShift;

				printf("Sync Mark gefunden: %x %x\n",*verifySectorData,verifySectorDataNextByteMask);

				return 0;
			}
		}
		verifySectorData++;
		verifySectorDataBytesLeft--;
	}
	return 1;
}

uint8_t floppy_raw_getNextCylinderBufferByte()
{
	uint8_t ret;

	if (verifySectorDataShift==0)
		ret = verifySectorData[0];
	else
		ret = ( verifySectorData[0] << verifySectorDataShift ) | (verifySectorData[1]>>(8-verifySectorDataShift));

	printf("%d %02x %02x -> %02x ",verifySectorDataShift,verifySectorData[0],verifySectorData[1],ret);
	printCharBin(ret);
	printf("\n");
	verifySectorData++;
	verifySectorDataBytesLeft--;

	return 0;
}


int main()
{
	cylinderSize=sizeof(cylinderBuffer1);
	cylinderBuffer=cylinderBuffer1;

	int i;
	for (i=0;i<cylinderSize;i++)
	{
		printCharBin(cylinderBuffer[i]);
		printf(" ");
	}

	printf("\n");

	floppy_raw_find1541Sync();
	floppy_raw_getNextCylinderBufferByte();
	floppy_raw_getNextCylinderBufferByte();
	floppy_raw_getNextCylinderBufferByte();

	printf("\n");
	printf("\n");
	
	cylinderSize=sizeof(cylinderBuffer2);
	cylinderBuffer=cylinderBuffer2;

	for (i=0;i<cylinderSize;i++)
	{
		printCharBin(cylinderBuffer[i]);
		printf(" ");
	}

	printf("\n");

	floppy_raw_find1541Sync();
	floppy_raw_getNextCylinderBufferByte();
	floppy_raw_getNextCylinderBufferByte();
	floppy_raw_getNextCylinderBufferByte();
	

}

