
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


#define RAWBLOCK_TYPE_HEAD 1
#define RAWBLOCK_TYPE_HAS_VARIABLE_DENSITY 2
#define RAWBLOCK_TYPE_TIME_DATA 4


int floppy_raw_writeTrack(int cylinder, int head)
{
	printf("floppy_writeRawTrack %d %d\n",cylinder,head);

	int i;
	uint8_t *trackData=NULL;
	uint8_t *timeData=NULL;

	static int raw_cellLengthDecrement=0;

	int trackDataSize=0;
	int timeDataSize=0;

	unsigned int timeDataCellLength=0;
	unsigned int timeDataCellReloadPos=0;
	int timeDataUsed=0;


	uint8_t *cylBufPtr=cylinderBuffer;
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

	assert (trackData >= &cylinderBuffer[0]);
	assert (trackData + trackDataSize < &cylinderBuffer[CYLINDER_BUFFER_SIZE]);

	printf("trackData %x %x\n",trackData[0],trackData[trackDataSize-1]);

	flux_mode=FLUX_MODE_MFM_ISO;

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

	if (head)
		assert(!(GPIOB->IDR & GPIO_Pin_11));
	else
		assert((GPIOB->IDR & GPIO_Pin_11));

	//mfm_cellLength-=8;//Turrican... Debug damit es schneller geht
	//mfm_decodeCellLength=151;//Debug damit es schneller geht
	do
	{
		if (floppy_waitForIndex())
			return 1;

		assert((GPIOB->IDR & GPIO_Pin_5));
		floppy_setWriteGate(1);


		//Ist das wirklich notwendig? Wir warten auf den Index und löschen die ganze Spur zur Sicherheit einmal...
		if (floppy_waitForIndex())
			return 1;

		assert(!(GPIOB->IDR & GPIO_Pin_5));

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

				assert (trackData >= &cylinderBuffer[0]);
				assert (trackData < &cylinderBuffer[CYLINDER_BUFFER_SIZE]);
			}
		}

		flux_blockedWrite(0x55);

		assert(!(GPIOB->IDR & GPIO_Pin_5));
		floppy_setWriteGate(0);
		assert((GPIOB->IDR & GPIO_Pin_5));

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
				printf("track overflown with %d %d\n",overflownBytes,mfm_decodeCellLength);
				//mfm_decodeCellLength--;
				raw_cellLengthDecrement++;
			}
		}

		//overflownBytes=0;


	}
	while (overflownBytes > 0);


	if (head)
		assert(!(GPIOB->IDR & GPIO_Pin_11));
	else
		assert((GPIOB->IDR & GPIO_Pin_11));


	assert(GPIOB->IDR & GPIO_Pin_5);

	flux_write_setEnableState(DISABLE);

	return 0;
}

