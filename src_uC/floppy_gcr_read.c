/*
 * floppy_gcr_read.c
 *
 *  Created on: 03.04.2016
 *      Author: andre
 */


#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_mfm.h"
#include "floppy_control.h"
#include "floppy_settings.h"

volatile static uint32_t shiftedBits=0;
volatile static uint32_t oneCounter=0;
volatile uint8_t rawGcrSaved = 0;
volatile static uint32_t rawGcr = 0;
volatile static uint32_t decodedGcr = 0;
volatile static uint32_t gcr_inSync = 0;
volatile static uint32_t gcr_decodedNibbleValid = 0;
volatile uint8_t gcr_decodedByte= 0;

static unsigned int gcr_timeOut=0;

extern volatile unsigned int diffCollectorEnabled;


//http://www.baltissen.org/newhtm/1541c.htm

const unsigned char gcrEncodeTable[]=
{
	0b01010, //0000
	0b01011, //0001
	0b10010, //0010
	0b10011, //0011
	0b01110, //0100
	0b01111, //0101
	0b10110, //0110
	0b10111, //0111

	0b01001, //1000
	0b11001, //1001
	0b11010, //1010
	0b11011, //1011
	0b01101, //1100
	0b11101, //1101
	0b11110, //1110
	0b10101  //1111
};

const unsigned char gcrDecodeTable[]=
{
	0xff, //00000
	0xff, //00001
	0xff, //00010
	0xff, //00011
	0xff, //00100
	0xff, //00101
	0xff, //00110
	0xff, //00111

	0xff, //01000
	0b1000, //01001
	0b0000, //01010
	0b0001, //01011
	0xff, //01100
	0b1100, //01101
	0b0100, //01110
	0b0101, //01111

	0xff, //10000
	0xff, //10001
	0b0010, //10010
	0b0011, //10011
	0xff, //10100
	0b1111, //10101
	0b0110, //10110
	0b0111, //10111

	0xff, //11000
	0b1001, //11001
	0b1010, //11010
	0b1011, //11011
	0xff, //11100
	0b1101, //11101
	0b1110, //11110
	0xff  //11111
};

void gcr_c64_crossVerifyCodeTables()
{
	unsigned char i;
	for (i=0; i<16; i++)
	{
		unsigned char encoded=gcrEncodeTable[i];
		unsigned char decoded=gcrDecodeTable[encoded];
		assert(decoded != 0xff);

		if (i!=decoded)
		{
			printf("Verify failed for i==%x %x %x\n",i,encoded,decoded);
			assert(0);
		}
		//assert(i==decoded);

	}

}


void gcr_c64_decode()
{
	/*
	printf("%08x %2d %d ",rawMFM,shiftedBits,mfm_inSync);
	printLongBin(rawMFM);
	printf("\n");
	 */

	if (geometry_format==FLOPPY_FORMAT_RAW_GCR)
	{
		if (gcr_inSync && shiftedBits==8)
		{
			shiftedBits=0;

			rawGcrSaved=rawGcr;
			rawGcr=0;
			//printf("raw:%2x\n",rawGcrSaved);
			gcr_decodedNibbleValid=1;

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
			flux_read_diffDebugFifoWrite(0x20000|rawGcrSaved);
#endif
		}
	}
	else
	{
		if (gcr_inSync && shiftedBits==5)
		{
			shiftedBits=0;
			//printf("Decoded:%2x\n",decodedMFM);
			decodedGcr=gcrDecodeTable[rawGcr & 0x1f];
			rawGcr=0;
			gcr_decodedNibbleValid=1;
		}
	}


}

void gcr_c64_5CellsNoTransitionHandler()
{
	int i;

	for (i=0; i< 5;i++)
	{
		if (oneCounter >= 10) //Eigentlich sollten es 10 sein...
		{
			shiftedBits=0;
			gcr_inSync=1;
		}
		oneCounter=0;

		rawGcr<<=1;
		shiftedBits++;
		gcr_c64_decode();
	}
}

void gcr_c64_transitionHandler()
{
	if (diff < 200)
	{
		gcr_inSync=0;
		oneCounter=0;
		STM_EVAL_LEDOn(LED4);
		STM_EVAL_LEDOff(LED4);
		return;
	}

	//if (diff > (mfm_decodeCellLength<<3))
	if (diff > MAXIMUM_VALUE)
	{
		//ignoriere Zeiten die länger sind als 8 Zellen
		return;
	}

	//Die leeren Zellen werden nun abgezogen und 0en werden eingeshiftet.
	//printf("diff:%d\n",diff);
	while (diff > mfm_decodeCellLength + mfm_decodeCellLength/2) //+mfm_cellLength/2 ist die Toleranz die genau auf die Mitte gesetzt wird.
	{
		if (oneCounter >= 10) //Eigentlich sollten es 10 sein...
		{
			shiftedBits=0;
			gcr_inSync=1;
		}
		oneCounter=0;

		diff-=mfm_decodeCellLength;
		rawGcr<<=1;
		shiftedBits++;
		gcr_c64_decode();
	}

	//Es bleibt eine 1 übrig.
	rawGcr<<=1;
	rawGcr|=1;
	shiftedBits++;
	oneCounter++;
	gcr_c64_decode();
#ifdef ACTIVATE_DIFFCOLLECTOR
	if (oneCounter >= 10)
		diffCollectorEnabled=1;
#endif

}

void gcr_blockedWaitForSyncState()
{
	gcr_timeOut=4000000;


	gcr_inSync=0;
	while (!gcr_inSync && gcr_timeOut)
	{
		ACTIVE_WAITING
		gcr_timeOut--;
	}

	if (!gcr_inSync)
	{
		printf("no sync timeout...\n");
		mfm_errorHappened=1;
	}

}


void gcr_blockedRead()
{
	gcr_timeOut=30000;

	//Das obere Nibble
	gcr_decodedNibbleValid=0;
	while (!gcr_decodedNibbleValid && gcr_timeOut)
	{
		ACTIVE_WAITING
		gcr_timeOut--;
	}

	if (!gcr_decodedNibbleValid || decodedGcr==0xff)
	{
		mfm_errorHappened=1;
		printf("gcr_blockedReadRawByte timeout\n");
		return;
	}

	gcr_decodedByte=decodedGcr<<4;

	//Das untere Nibble
	gcr_decodedNibbleValid=0;
	while (!gcr_decodedNibbleValid && gcr_timeOut)
	{
		ACTIVE_WAITING
		gcr_timeOut--;
	}

	if (!gcr_decodedNibbleValid || decodedGcr==0xff)
	{
		mfm_errorHappened=1;
		printf("gcr_blockedReadRawByte timeout\n");
		return;
	}

	gcr_decodedByte|=decodedGcr;
}


void gcr_blockedReadRawByte()
{
	gcr_timeOut=30000;

	//Das obere Nibble
	gcr_decodedNibbleValid=0;
	while (!gcr_decodedNibbleValid && gcr_timeOut)
	{
		ACTIVE_WAITING
		gcr_timeOut--;
	}

	if (!gcr_decodedNibbleValid)
	{
		mfm_errorHappened=1;
		printf("gcr_blockedReadRawByte timeout\n");
	}

}


