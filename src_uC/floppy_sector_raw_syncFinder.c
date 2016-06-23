#include <stdio.h>

#include "floppy_mfm.h"
#include "floppy_sector.h"
#include "floppy_control.h"
#include "floppy_settings.h"
#include "floppy_sector_raw.h"
#include "assert.h"


#define VERIFY_PARTS_ANZ 23
uint8_t *verifySectorData;
int verifySectorDataBytesLeft;
int verifySectorDataShift=0;
int verifySectorDataNextByteMask=0;

struct
{
	uint8_t *ptr;
	int len;
	int dataShift;
	int nextByteMask;
} verifyablePart[VERIFY_PARTS_ANZ];

int verifyablePartI=0;

void floppy_raw_getNextVerifyablePart()
{
	verifySectorData=verifyablePart[verifyablePartI].ptr;
	verifySectorDataBytesLeft=verifyablePart[verifyablePartI].len;
	verifySectorDataShift=verifyablePart[verifyablePartI].dataShift;
	verifySectorDataNextByteMask=verifyablePart[verifyablePartI].nextByteMask;
	verifyablePartI++;
}

void floppy_raw_findMFMSync()
{
	//wir suchen nach dem Muster * 11111 11111 0
	int i,j;
	uint32_t wordAccu=0;
	uint32_t byteAccu=0;
	int inSync=0;

	verifyablePartI=0;

	for (i=0; i < trackDataSize; i++)
	{

		byteAccu = trackData[i];
		if (inSync)
		{
			for (j=0; j < 8; j++)
			{
				wordAccu<<=1;

				if (byteAccu & 0x80)
					wordAccu|=1;

				byteAccu<<=1;

				//Wenn wir im Sync sind, dürfen keine 4 Null Bits aufeinander folgen!

				if ((wordAccu & 0b1111)==0)
				{
					inSync=0;
					verifyablePart[verifyablePartI].len=&trackData[i]-verifyablePart[verifyablePartI].ptr-1;

					//nur gerade anzahl an bytes sind erlaubt
					verifyablePart[verifyablePartI].len = (verifyablePart[verifyablePartI].len & ~1);

					printf("Sync loss: %d\n",verifyablePart[verifyablePartI].len);
					verifyablePartI++;
					assert(verifyablePartI<VERIFY_PARTS_ANZ);
					break;
				}
			}
		}
		else
		{
			for (j=0; j < 8; j++)
			{
				wordAccu<<=1;

				if (byteAccu & 0x80)
					wordAccu|=1;

				byteAccu<<=1;

				//Wir suchen nach einem Sync Mark. Das sind >10 gesetzte Bits von einem 0 Bit.
				if ((wordAccu & 0x1ffff)==(SYNC_WORD_ISO<<1))
				{
					verifySectorDataShift=j;
					verifySectorDataNextByteMask=0xff >> verifySectorDataShift;

					printf("Sync: %d %x %x %x\n",i,trackData[i],verifySectorDataNextByteMask,verifySectorDataShift);
					inSync=1;

					verifyablePart[verifyablePartI].ptr=&trackData[i];
					verifyablePart[verifyablePartI].dataShift=verifySectorDataShift;
					verifyablePart[verifyablePartI].nextByteMask=verifySectorDataNextByteMask;
				}
			}
		}
	}

	if (inSync)
	{
		verifyablePart[verifyablePartI].len=&trackData[i]-verifyablePart[verifyablePartI].ptr-1;

		//nur gerade anzahl an bytes sind erlaubt
		verifyablePart[verifyablePartI].len = (verifyablePart[verifyablePartI].len & ~1);

		printf("End: %d\n",verifyablePart[verifyablePartI].len);
		verifyablePartI++;
	}
	assert(verifyablePartI<VERIFY_PARTS_ANZ);

	verifyablePart[verifyablePartI].ptr=0;
	verifyablePartI=0;
}

void floppy_raw_find1541Sync()
{
	//wir suchen nach dem Muster * 11111 11111 0
	int i,j;
	uint32_t wordAccu=0;
	uint32_t byteAccu=0;
	int inSync=0;

	verifyablePartI=0;

	for (i=0; i < trackDataSize; i++)
	{

		byteAccu = trackData[i];
		if (inSync)
		{
			for (j=0; j < 8; j++)
			{
				wordAccu<<=1;

				if (byteAccu & 0x80)
					wordAccu|=1;

				byteAccu<<=1;

				//Wenn wir im Sync sind, dürfen keine 3 Null Bits aufeinander folgen!

				if ((wordAccu & 0b111)==0)
				{
					inSync=0;
					verifyablePart[verifyablePartI].len=&trackData[i]-verifyablePart[verifyablePartI].ptr-1;
					printf("Sync loss: %d\n",verifyablePart[verifyablePartI].len);
					verifyablePartI++;
					assert(verifyablePartI<VERIFY_PARTS_ANZ);
					break;
				}
			}
		}
		else
		{
			for (j=0; j < 8; j++)
			{
				wordAccu<<=1;

				if (byteAccu & 0x80)
					wordAccu|=1;

				byteAccu<<=1;

				//Wir suchen nach einem Sync Mark. Das sind >10 gesetzte Bits von einem 0 Bit.
				if ((wordAccu & 0b11111111111)==0b11111111110)
				{

					verifySectorDataShift=j;
					verifySectorDataNextByteMask=0xff >> verifySectorDataShift;

					printf("Sync: %d %x %x %x\n",i,trackData[i],verifySectorDataNextByteMask,verifySectorDataShift);
					inSync=1;

					verifyablePart[verifyablePartI].ptr=&trackData[i];
					verifyablePart[verifyablePartI].dataShift=verifySectorDataShift;
					verifyablePart[verifyablePartI].nextByteMask=verifySectorDataNextByteMask;
				}
			}
		}
	}

	if (inSync)
	{
		verifyablePart[verifyablePartI].len=&trackData[i]-verifyablePart[verifyablePartI].ptr-1;
		printf("End: %d\n",verifyablePart[verifyablePartI].len);
		verifyablePartI++;
	}
	assert(verifyablePartI<VERIFY_PARTS_ANZ);

	verifyablePart[verifyablePartI].ptr=0;
	verifyablePartI=0;
}

uint8_t floppy_raw_getNextCylinderBufferByte()
{
	uint8_t ret;

	if (verifySectorDataShift==0)
		ret = verifySectorData[0];
	else
		ret = ( verifySectorData[0] << verifySectorDataShift ) | (verifySectorData[1]>>(8-verifySectorDataShift));

	//printf("%d %02x %02x -> %02x ",verifySectorDataShift,verifySectorData[0],verifySectorData[1],ret);
	//printCharBin(ret);
	//printf("\n");
	verifySectorData++;
	verifySectorDataBytesLeft--;

	return ret;
}

