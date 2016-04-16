/*
 * floppy_mfm_read.c
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

volatile uint32_t mfm_savedRawWord=0;
volatile uint8_t mfm_decodedByte=0;
volatile uint32_t mfm_inSync=0;
volatile uint32_t mfm_decodedByteValid=0;

volatile static uint32_t rawMFM = 0;
volatile static uint8_t decodedMFM = 0;
volatile static uint32_t shiftedBits=0;
int indexOverflowCount=0;

static unsigned int mfm_timeOut=0;
unsigned int mfm_errorHappened=0;

//#define SYNC_WORD 0x5224
#define SYNC_WORD_ISO 0x4489 //broken A1
#define SYNC_WORD_AMIGA 0x44894489ul //broken A1 *2


void mfm_iso_decode()
{
	//printf("%4x %2d ",rawMFM,shiftedBits);
	//printShortBin(rawMFM);
	//printf(" ");
	//printCharBin(decodedMFM);
	//printf("\n");

	if (mfm_inSync && shiftedBits==16)
	{
		shiftedBits=0;
		//printf("Decoded:%2x\n",decodedMFM);
		mfm_decodedByte=decodedMFM;
		mfm_savedRawWord=rawMFM;
		mfm_decodedByteValid=1;
	}

	if ((rawMFM & 0xffff)==SYNC_WORD_ISO) //IAM sync word is broken A1.
	{
		//STM_EVAL_LEDOn(LED3);
		shiftedBits=0;
		rawMFM=0;
		decodedMFM=0;

		mfm_inSync++;
		if (mfm_inSync>5)
			mfm_inSync=5;
		//STM_EVAL_LEDOff(LED3);

#ifdef ACTIVATE_DIFFCOLLECTOR
		diffCollectorEnabled=1;
#endif
	}
}

void mfm_iso_transitionHandler()
{
	if (diff > MAXIMUM_VALUE)
	{
		//Die letzte Transition ist zu weit weg. Desynchronisiert...
		mfm_inSync=0;
		//printf("UNSYNC\n");
	}
	else
	{
		//Die leeren Zellen werden nun abgezogen und 0en werden eingeshiftet.
		//printf("diff:%d\n",diff);
		while (diff > mfm_decodeCellLength + mfm_decodeCellLength/2) //+mfm_cellLength/2 ist die Toleranz die genau auf die Mitte gesetzt wird.
		{
			diff-=mfm_decodeCellLength;
			rawMFM<<=1;
			if ((shiftedBits&1)!=0)
			{
				decodedMFM<<=1;
			}
			shiftedBits++;
			mfm_iso_decode();
		}

		//Es bleibt eine 1 übrig.
		rawMFM<<=1;
		rawMFM|=1;
		if ((shiftedBits&1)!=0)
		{
			//Die geraden Bits bestimmen die tatsächlichen Daten.
			decodedMFM<<=1;
			decodedMFM|=1;
		}
		shiftedBits++;
		mfm_iso_decode();
	}
}



void mfm_amiga_decode()
{
	/*
	printf("%08x %2d %d ",rawMFM,shiftedBits,mfm_inSync);
	printLongBin(rawMFM);
	printf("\n");
	 */

	if (mfm_inSync && shiftedBits==32)
	{
		shiftedBits=0;
		//printf("Decoded:%2x\n",decodedMFM);
		mfm_savedRawWord=rawMFM;
		mfm_decodedByteValid=1;
	}

	if (rawMFM==SYNC_WORD_AMIGA) //IAM sync word is broken A1 *2.
	//if ((rawMFM & 0xffff)==SYNC_WORD_ISO) //IAM sync word is broken A1.
	{
		//printf("Sync Word\n");
		shiftedBits=0;
		rawMFM=0;
		mfm_inSync++;
		if (mfm_inSync>5)
			mfm_inSync=5;
	}
}

void mfm_amiga_transitionHandler()
{
	if (diff > MAXIMUM_VALUE)
	{
		//Die letzte Transition ist zu weit weg. Desynchronisiert...
		mfm_inSync=0;
		//printf("UNSYNC\n");
	}
	else
	{
		//Die leeren Zellen werden nun abgezogen und 0en werden eingeshiftet.
		//printf("diff:%d\n",diff);
		while (diff > mfm_decodeCellLength + mfm_decodeCellLength/2) //+mfm_cellLength/2 ist die Toleranz die genau auf die Mitte gesetzt wird.
		{
			diff-=mfm_decodeCellLength;
			rawMFM<<=1;
			shiftedBits++;
			mfm_amiga_decode();
		}

		//Es bleibt eine 1 übrig.
		rawMFM<<=1;
		rawMFM|=1;
		shiftedBits++;
		mfm_amiga_decode();
	}
}


void mfm_blockedWaitForSyncWord(int expectNum)
{
	mfm_timeOut=400000;

	//STM_EVAL_LEDOff(LED4);
	while (mfm_inSync!=expectNum && mfm_timeOut)
	{
		ACTIVE_WAITING
		mfm_timeOut--;
	}

	if (mfm_inSync!=expectNum)
		mfm_errorHappened=1;

	/*
	else
	{
		STM_EVAL_LEDOn(LED4);
	}
	*/
}


void mfm_blockedRead()
{
	mfm_timeOut=30000;
	mfm_decodedByteValid=0;

	while (!mfm_decodedByteValid && mfm_timeOut)
	{
		ACTIVE_WAITING
		mfm_timeOut--;
	}

	if (!mfm_decodedByteValid)
		mfm_errorHappened=1;
}



