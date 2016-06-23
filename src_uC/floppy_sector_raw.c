
#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_crc.h"
#include "floppy_mfm.h"
#include "floppy_sector.h"
#include "floppy_control.h"
#include "floppy_settings.h"
#include "assert.h"
#include "floppy_sector_raw.h"

#define RAWBLOCK_TYPE_HEAD 1
#define RAWBLOCK_TYPE_HAS_VARIABLE_DENSITY 2
#define RAWBLOCK_TYPE_TIME_DATA 4

int trackDataSize=0;
uint8_t *trackData=NULL;
int verifiedBytes=0;

void floppy_raw_printTrack()
{
	int i;
	for (i=0; i< trackDataSize; i++)
	{
		if ((i%16)==0)
			printf("\n %06d ",i);

		printf("%02x ",trackData[i]);

	}
	printf("\n");
}

int floppy_raw_writeTrack(int cylinder, int head)
{
	//printf("floppy_writeRawTrack %d %d\n",cylinder,head);

	int i;

	uint8_t *timeData=NULL;

	static int raw_cellLengthDecrement=0;

	int timeDataSize=0;

	unsigned int timeDataCellLength=0;
	unsigned int timeDataCellReloadPos=0;
	int timeDataUsed=0;
	trackDataSize=0;
	trackData=NULL;

	uint8_t *cylBufPtr=(uint8_t*)cylinderBuffer;
	while ( (timeDataUsed && (!trackData || !timeData)) || (!timeDataUsed && !trackData) )
	{
		int blocklen=((int)cylBufPtr[0]<<8) | cylBufPtr[1];

		if (blocklen==0)
		{
			printf("Konnte nicht alle notwendigen Daten finden!\n");
			return 1;
		}

		int blocktype=cylBufPtr[2];

		printf("Found block %d %d\n",blocklen,cylBufPtr[2]);

		if ((blocktype & RAWBLOCK_TYPE_HEAD) == head)
		{
			if (blocktype & RAWBLOCK_TYPE_HAS_VARIABLE_DENSITY)
				timeDataUsed=1;

			if (blocktype & RAWBLOCK_TYPE_TIME_DATA)
			{
				timeData=cylBufPtr+3;
				timeDataSize=blocklen;
				printf("timeData starts\n");
			}
			else
			{

				trackData=cylBufPtr+3;
				trackDataSize=blocklen;
				printf("Trackdata starts with %x\n",*trackData);
			}
		}

		cylBufPtr+=3+blocklen;

	}
	printf("floppy_writeRawTrack %d %d %d %d %d\n",cylinder,head,trackDataSize,timeDataSize,timeDataUsed);

	assert (trackData >= (unsigned char*)&cylinderBuffer[0]);
	assert (trackData + trackDataSize < (unsigned char*)&cylinderBuffer[CYLINDER_BUFFER_SIZE]);

	//printf("trackData %x %x\n",trackData[0],trackData[trackDataSize-1]);

	//flux_mode=FLUX_MODE_MFM_ISO;

	//Das erste Byte muss per ENCODE übertragen werden. Ein leeres CurrentWord lässt den Automaten abstürzen!
	//mfm_configureWrite(MFM_ENCODE,8);
	//mfm_configureWrite(MFM_ENCODE,8);

	if (timeDataUsed)
	{
		assert(timeData!=NULL);

		/*
		for (i=0; i<  timeDataSize; i++)
		{
			printf("%02x ",timeData[i]);
			if ((i%16)==15)
				printf("\n");
		}
		printf("\n");
		*/

		assert(timeData[0]==0); //der erste Eintrag muss mit dem ersten Datenbyte anfangen
		assert(timeData[1]==0);

		timeDataCellLength=((int)timeData[2]<<8)|(int)timeData[3];
		timeDataCellReloadPos=((int)timeData[4]<<8)|(int)timeData[5];
		timeData+=6;
		//timeDataCellLength=MFM_BITTIME_DD*timeDataCellLength/2000;

		flux_configureWrite(FLUX_RAW,8);
		flux_configureWriteCellLength(timeDataCellLength);
		mfm_decodeCellLength = timeDataCellLength;
		printf("cellLength %d\n",timeDataCellLength);
	}
	else
	{
		flux_configureWrite(FLUX_RAW,8);
		flux_configureWriteCellLength(0);
	}

	flux_read_setEnableState(DISABLE);
	flux_write_setEnableState(ENABLE);

	int overflownBytes;

#ifndef CUNIT
	if (head)
		assert(!(GPIOB->IDR & GPIO_Pin_11));
	else
		assert((GPIOB->IDR & GPIO_Pin_11));
#endif

	//mfm_cellLength-=8;//Turrican... Debug damit es schneller geht
	//mfm_decodeCellLength=151;//Debug damit es schneller geht

#ifdef ACTIVATE_DEBUG_FLUX_WRITE_FIFO
	int zeroHappened=0;
#endif

	do
	{
		if (floppy_waitForIndex())
			return 1;

#ifndef CUNIT
		assert((GPIOB->IDR & GPIO_Pin_5));
#endif

		floppy_setWriteGate(1);

		//Ist das wirklich notwendig? Wir warten auf den Index und löschen die ganze Spur zur Sicherheit einmal...
		if (floppy_waitForIndex())
			return 1;

		/*
		if (floppy_waitForIndex())
			return 1;

		if (floppy_waitForIndex())
			return 1;

		if (floppy_waitForIndex())
			return 1;

		*/

#ifndef CUNIT
		assert(!(GPIOB->IDR & GPIO_Pin_5));
#endif

		overflownBytes=0;

		/*
		for (i=0; i<  trackDataSize; i++)
		{
			printf("%02x ",trackData[i]);
			if ((i%16)==15)
				printf("\n");
		}
		printf("\n");
		*/

		/*
		if (head)
		{
			for (i=0; i < trackSize; i++)
			{
				mfm_blockedWrite(0xaa);

				if (indexHappened)
					overflownBytes++;
			}
		}
		else
		*/
#ifdef ACTIVATE_DEBUG_FLUX_WRITE_FIFO
		fluxWriteDebugFifo_enabled = 1;
#endif

		{
			for (i=0; i < trackDataSize; i++)
			{

				if (timeDataUsed && i==timeDataCellReloadPos)
				{
					//printf("Reload timedata %d\n",i);
					timeDataCellLength=((int)timeData[0]<<8)|(int)timeData[1];
					timeDataCellReloadPos=((int)timeData[2]<<8)|(int)timeData[3];
					timeData+=4;
					//timeDataCellLength=MFM_BITTIME_DD*timeDataCellLength/2000;

					flux_configureWriteCellLength(timeDataCellLength);

				}
#if 0
				if (!timeDataUsed)
				{
					if (raw_cellLengthDecrement&1)
					{
						if (i&1)
							flux_configureWriteCellLength(mfm_decodeCellLength - (raw_cellLengthDecrement>>1) -1);
						else
							flux_configureWriteCellLength(mfm_decodeCellLength - (raw_cellLengthDecrement>>1));
					}
					else
						flux_configureWriteCellLength(mfm_decodeCellLength - (raw_cellLengthDecrement>>1));
				}
#endif


				flux_blockedWrite(trackData[i]);

#ifdef ACTIVATE_DEBUG_FLUX_WRITE_FIFO
				if (trackData[i]==0 && !zeroHappened)
				{
					zeroHappened=1;
				}

				if (zeroHappened)
				{
					zeroHappened++;
					if (zeroHappened==20)
						fluxWriteDebugFifo_enabled=0;
				}
#endif

				if (!indexHappened)
					assert(overflownBytes==0);

				if (indexHappened)
					overflownBytes++;

				/*
				assert(!(GPIOB->IDR & GPIO_Pin_5));

				if (head)
					assert(!(GPIOB->IDR & GPIO_Pin_11));
				else
					assert((GPIOB->IDR & GPIO_Pin_11));
				*/

				assert (&trackData[i] >= (unsigned char*)&trackData[0]);
				assert (&trackData[i] < (unsigned char*)&trackData[trackDataSize]);
			}
		}

		flux_blockedWrite(0x55);
		flux_write_waitForUnderflow();

#if 0
		flux_write_setEnableState(DISABLE);
		uint16_t tmpccmr2 = 0;
		tmpccmr2 = TIM4->CCMR2;

		for (o=15; o < 40; o++)
		{

			for (i=0; i < 200; i++)
			{
				/* Reset the OC1M Bits */
				tmpccmr2 &= (uint16_t)~TIM_CCMR2_OC3M;

				/* Configure The Forced output Mode */
				tmpccmr2 |= TIM_ForcedAction_InActive;

				/* Write to TIMx CCMR2 register */
				TIM4->CCMR2 = tmpccmr2;

				for (j=0;j<o;j++)
					__ASM volatile ("nop");

				/* Reset the OC1M Bits */
				tmpccmr2 &= (uint16_t)~TIM_CCMR2_OC3M;

				/* Configure The Forced output Mode */
				tmpccmr2 |= TIM_ForcedAction_Active;

				/* Write to TIMx CCMR2 register */
				TIM4->CCMR2 = tmpccmr2;

				for (j=0;j<15;j++)
					__ASM volatile ("nop");



				/* Reset the OC1M Bits */
				tmpccmr2 &= (uint16_t)~TIM_CCMR2_OC3M;

				/* Configure The Forced output Mode */
				tmpccmr2 |= TIM_ForcedAction_InActive;

				/* Write to TIMx CCMR2 register */
				TIM4->CCMR2 = tmpccmr2;

				for (j=0;j<10;j++)
					__ASM volatile ("nop");



				/* Reset the OC1M Bits */
				tmpccmr2 &= (uint16_t)~TIM_CCMR2_OC3M;

				/* Configure The Forced output Mode */
				tmpccmr2 |= TIM_ForcedAction_Active;

				/* Write to TIMx CCMR2 register */
				TIM4->CCMR2 = tmpccmr2;

				for (j=0;j<15;j++)
					__ASM volatile ("nop");
			}
		}

		/* Reset the OC1M Bits */
		tmpccmr2 &= (uint16_t)~TIM_CCMR2_OC3M;

		/* Configure The Active output Mode */
		tmpccmr2 |= TIM_OCMode_Active;

		/* Write to TIMx CCMR2 register */
		TIM4->CCMR2 = tmpccmr2;

		flux_write_setEnableState(ENABLE);
#endif

#ifndef CUNIT
		assert(!(GPIOB->IDR & GPIO_Pin_5));
#endif

		floppy_setWriteGate(0);

#ifndef CUNIT
		assert((GPIOB->IDR & GPIO_Pin_5));
#endif

		//Wir hängen noch ein paar Bytes ran, damit ein GAP entstehen kann.

		for (i=0; i < 35; i++)
		{
			flux_blockedWrite(0x55);

			if (indexHappened)
				overflownBytes++;
		}


		if (overflownBytes)
		{
			if (timeDataUsed)
			{
				printf("track overflown with %d and timeData.. ill accept... *************\n",overflownBytes);
				overflownBytes=0;
			}
			else
			{
				printf("track overflown with %d %d\n",(int)overflownBytes,(int)mfm_decodeCellLength);
				//mfm_decodeCellLength--;
				raw_cellLengthDecrement++;
			}
		}

		//overflownBytes=0;


	}
	while (overflownBytes > 0);

#ifdef ACTIVATE_DEBUG_FLUX_WRITE_FIFO
	if (zeroHappened)
	{
		printFluxWriteDebugFifo();
	}
#endif

#ifndef CUNIT
	if (head)
		assert(!(GPIOB->IDR & GPIO_Pin_11));
	else
		assert((GPIOB->IDR & GPIO_Pin_11));

	assert(GPIOB->IDR & GPIO_Pin_5);
#endif

	flux_write_setEnableState(DISABLE);

	return 0;
}


int floppy_raw_gcr_readTrackMachine()
{
	if (mfm_errorHappened)
	{
		//printf("R\n");
		mfm_errorHappened=0;
		trackReadState=0;
	}

	switch (trackReadState)
	{
	case 0:
		floppy_raw_find1541Sync();

		verifiedBytes=0;

		//wenn Flippy mode aktiv, wird kein warten auf index notwendig sein.
		if (configuration_flags & CONFIGFLAG_FLIPPY_SIMULATE_INDEX)
			trackReadState=2;
		else
			trackReadState++;
		break;
	case 1:
		if (!floppy_waitForIndex())
			trackReadState++;
		break;
	case 2:
		floppy_raw_getNextVerifyablePart();

		if (!verifySectorData)
		{
			//printf("Compared %d byte\n",verifiedBytes);
			printf("\t->\tCompared %d percent\n",verifiedBytes*100/trackDataSize);
			sectorsRead=1;
			trackReadState=5;
		}
		else
			trackReadState++;
		break;
	case 3:

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
		flux_read_diffDebugFifoWrite(0x100000);
#endif

		gcr_blockedWaitForSyncState();

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
		flux_read_diffDebugFifoWrite(0x200000);
#endif

		trackReadState++;
		break;
	case 4:
	{
		uint8_t toCompare=floppy_raw_getNextCylinderBufferByte();

		gcr_blockedReadRawByte();
		uint8_t readBack=rawGcrSaved;

		//gcr_blockedRead();

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
		flux_read_diffDebugFifoWrite(0x40000 | (rawGcrSaved<<8) | toCompare);
#endif

		if (readBack!=toCompare)
		{
			//if ((configuration_flags & CONFIGFLAG_FLIPPY_SIMULATE_INDEX) && verifyablePartI==1)
			if ((configuration_flags & CONFIGFLAG_FLIPPY_SIMULATE_INDEX))
			{
				//im Flippy mode ist das Index Signal leider etwas schlecht. wir verzeihen den fehler, sofern es der Anfang der Spur ist und
				//versuchen es nochmal in der hoffnung, dass dann der richtige Bereich gefunden wird.

				verifyablePartI--;
				trackReadState=2;
			}
			else
			{
				//wenn kein flippy vorliegt, ist dies auf jeden fall ein Fehler!

				STM_EVAL_LEDOn(LED4);

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
				printDebugDiffFifo();
#endif

				printf("raw verify failed: %x %d %x != %x\n",
					*(verifySectorData-1),
					verifySectorData-1-(uint8_t*)trackData,
					readBack,
					toCompare);

				floppy_raw_printTrack();
				STM_EVAL_LEDOff(LED4);
				return 2;
			}

		}
		else
			verifiedBytes++;

		if (!verifySectorDataBytesLeft)
		{
			trackReadState=2;
		}

		break;
	}
	case 5:
		printf("you idiot! i'm finished!\n");
		assert(0);
		break;

	}

	return 0;
}



int floppy_raw_mfm_readTrackMachine()
{
	if (mfm_errorHappened)
	{
		//printf("R\n");
		mfm_errorHappened=0;
		trackReadState=0;
	}

	switch (trackReadState)
	{
	case 0:
		floppy_raw_findMFMSync();

		verifiedBytes=0;
		trackReadState++;

		break;
	case 1:
		if (!floppy_waitForIndex())
			trackReadState++;
		break;

	case 2:
		floppy_raw_getNextVerifyablePart();

		if (!verifySectorData)
		{
			//printf("Compared %d byte\n",verifiedBytes);
			printf("\t->\tCompared %d percent\n",verifiedBytes*100/trackDataSize);
			sectorsRead=1;
			trackReadState=5;
		}
		else
			trackReadState++;
		break;
	case 3:

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
		flux_read_diffDebugFifoWrite(0x100000);
#endif

		//Sowohl Amiga, als auch ISO verwenden >=2 Sync words
		mfm_blockedWaitForSyncWord(1);
		//mfm_blockedWaitForSyncWord(2);


#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
		flux_read_diffDebugFifoWrite(0x200000);
#endif

		trackReadState++;
		break;
	case 4:
	{
		uint16_t toCompare=((uint16_t)floppy_raw_getNextCylinderBufferByte())<<8;
		toCompare|=floppy_raw_getNextCylinderBufferByte();

		mfm_blockedRead();
		uint16_t readBack=mfm_savedRawWord;

		//gcr_blockedRead();

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
		flux_read_diffDebugFifoWrite(0x40000 | (rawGcrSaved<<8) | toCompare);
#endif

		if (readBack!=toCompare)
		{
			//if ((configuration_flags & CONFIGFLAG_FLIPPY_SIMULATE_INDEX) && verifyablePartI==1)
			if ((configuration_flags & CONFIGFLAG_FLIPPY_SIMULATE_INDEX))
			{
				//im Flippy mode ist das Index Signal leider etwas schlecht. wir verzeihen den fehler, sofern es der Anfang der Spur ist und
				//versuchen es nochmal in der hoffnung, dass dann der richtige Bereich gefunden wird.

				verifyablePartI--;
				trackReadState=2;
			}
			else
			{
				//wenn kein flippy vorliegt, ist dies auf jeden fall ein Fehler!

				STM_EVAL_LEDOn(LED4);

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
				printDebugDiffFifo();
#endif

				printf("raw verify failed: %x %d %x != %x\n",
					*(verifySectorData-1),
					verifySectorData-1-(uint8_t*)trackData,
					readBack,
					toCompare);

				floppy_raw_printTrack();
				STM_EVAL_LEDOff(LED4);
				return 2;
			}

		}
		else
		{
			verifiedBytes+=2;
		}

		if (!verifySectorDataBytesLeft)
		{
			trackReadState=2;
		}

		break;
	}
	case 5:
		printf("you idiot! i'm finished!\n");
		assert(0);
		break;

	}

	return 0;
}



int floppy_raw_readTrackMachine()
{
	if (geometry_format == FLOPPY_FORMAT_RAW_GCR)
		return floppy_raw_gcr_readTrackMachine();
	else if (geometry_format == FLOPPY_FORMAT_RAW_MFM)
		return floppy_raw_mfm_readTrackMachine();

	assert(0);
	return 3;
}

