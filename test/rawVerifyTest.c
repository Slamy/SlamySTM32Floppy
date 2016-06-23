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

#include "floppy_sector_raw.h"

//GCR Tests
unsigned char cylinderBuffer1[]={0xff, 0xff, 0xff, 0xff, 0x92, 0x92, 0x93 ,0xff, 0x88};
unsigned char cylinderBuffer2[]={0xff, 0xff, 0xff, 0xff, 0x12, 0x12, 0x23 ,0xff, 0x88};

//MFM Tests
unsigned char mfmBuffer[]={0xaa, 0x44, 0x89, 0x44, 0x89, 0x55, 0x2a, 0xaa, 0xa5 ,0x55 ,0x2a, 0xa4};
unsigned char mfmBuffer2[]={0x95, 0x21, 0x2a, 0xaa, 0x51, 0x52, 0x52, 0x55, 0x54, 0x54, 0x52, 0x44 };


int trackDataSize=0;
uint8_t *trackData=NULL;


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



int main()
{

	int i;

	//Test 1
	/*
	trackDataSize=sizeof(cylinderBuffer1);
	trackData=cylinderBuffer1;

	for (i=0;i<trackDataSize;i++)
	{
		printCharBin(trackData[i]);
		printf(" ");
	}

	printf("\n");

	floppy_raw_find1541Sync();
	floppy_raw_getNextVerifyablePart();
	printf("%02x\n",floppy_raw_getNextCylinderBufferByte());
	printf("%02x\n",floppy_raw_getNextCylinderBufferByte());

	printf("\n");
	printf("\n");
	
	//Test 2
	trackDataSize=sizeof(cylinderBuffer2);
	trackData=cylinderBuffer2;

	for (i=0;i<trackDataSize;i++)
	{
		printCharBin(trackData[i]);
		printf(" ");
	}

	printf("\n");

	floppy_raw_find1541Sync();
	floppy_raw_getNextVerifyablePart();
	printf("%02x\n",floppy_raw_getNextCylinderBufferByte());
	printf("%02x\n",floppy_raw_getNextCylinderBufferByte());
	
	printf("\n");
	printf("\n");
*/
	//Test 3
	trackDataSize=sizeof(mfmBuffer);
	trackData=mfmBuffer;

	for (i=0;i<trackDataSize;i++)
	{
		printCharBin(trackData[i]);
		printf(" ");
	}

	printf("\n");

	floppy_raw_findMFMSync();
	floppy_raw_getNextVerifyablePart();
	printf("%02x\n",floppy_raw_getNextCylinderBufferByte());
	printf("%02x\n",floppy_raw_getNextCylinderBufferByte());
	printf("%02x\n",floppy_raw_getNextCylinderBufferByte());

	printf("\n");
	printf("\n");

	//Test 4
	trackDataSize=sizeof(mfmBuffer2);
	trackData=mfmBuffer2;

	for (i=0;i<trackDataSize;i++)
	{
		printCharBin(trackData[i]);
		printf(" ");
	}

	printf("\n");

	floppy_raw_findMFMSync();
	floppy_raw_getNextVerifyablePart();
	printf("%02x\n",floppy_raw_getNextCylinderBufferByte());
	printf("%02x\n",floppy_raw_getNextCylinderBufferByte());

}

