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

//arm-none-eabi-cpp floppy_sector.c -I CMSIS/ -IUtilities/ -I STM32F4xx_StdPeriph_Driver/inc -I pt-1.4/

//unsigned char trackBuffer[SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER];

uint32_t trackBuffer[CYLINDER_BUFFER_SIZE / 4];

unsigned int trackReadState=0;
unsigned int sectorsRead=0;
unsigned char trackSectorRead[MAX_SECTORS_PER_CYLINDER];
unsigned int sectorsDetected=0;
unsigned char trackSectorDetected[MAX_SECTORS_PER_CYLINDER];

unsigned char lastSectorDataFormat=0;
unsigned int verifyMode=0;


char *formatStr[]=
{
	"UNKNOWN",
	"Iso DD",
	"Iso HD",
	"Amiga DD",
	"",
	"",
	""
};

enum floppyFormat floppy_discoverFloppyFormat(int cylinder, int head)
{
	int failCnt;
	enum floppyFormat flopfrmt;

	//Wir versuchen es zuerst mit HD, dann mit DD
	floppy_stepToCylinder00();
	floppy_stepToCylinder(cylinder);
	floppy_setHead(head);
	mfm_read_setEnableState(ENABLE);

	for (flopfrmt=FLOPPY_FORMAT_ISO_DD; flopfrmt <= FLOPPY_FORMAT_AMIGA_DD; flopfrmt++)
	{
		floppy_configureFormat(flopfrmt,0,0,0);

		setupStepTimer(10000);

		failCnt=0;
		floppy_readTrackMachine_init();

		while (failCnt < 2)
		{

			if(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
			{
				//printf("TO\n");
				setupStepTimer(10000);
				failCnt++;
			}

			if (mfm_mode==MFM_MODE_AMIGA)
				floppy_amiga_readTrackMachine(cylinder,head);
			else
				floppy_iso_readTrackMachine(cylinder, head);
		}

		printf("Aborted with results: %d %d 0x%x\n",sectorsDetected,flopfrmt,lastSectorDataFormat);

		if (sectorsDetected >= 5 && sectorsRead >= 3)
		{
			printf("Format: %s\n",formatStr[flopfrmt]);
			return flopfrmt;
		}

	}

	mfm_read_setEnableState(DISABLE);

	return FLOPPY_FORMAT_UNKNOWN;

}

void floppy_readTrackMachine_init()
{
	int i;
	sectorsRead=0;
	sectorsDetected=0;
	for (i=0;i<MAX_SECTORS_PER_CYLINDER;i++)
	{
		trackSectorRead[i]=0;
		trackSectorDetected[i]=0;
	}

	mfm_errorHappened=0;
	trackReadState=0;
	lastSectorDataFormat=0;
	verifyMode=0;
}


int floppy_writeAndVerifyTrack(int cylinder, int head)
{
	unsigned int i=0;
	int try=0;
	int failCnt=0;
	unsigned int abortVerify=0;

	for (try=0; try < 3; try++)
	{
		//printf("Write... %d %d\n",cylinder,head);
		mfm_write_setEnableState(ENABLE);
		//printf("mfm_write enabled\n");

		if (mfm_mode == MFM_MODE_AMIGA)
		{
			if (floppy_amiga_writeTrack(cylinder,head,0))
				return 2;
		}
		else
		{
			if (floppy_iso_writeTrack(cylinder,head,0))
				return 2;
		}

		mfm_write_setEnableState(DISABLE);
		printf("Verify...\n");
		setupStepTimer(10000);

		failCnt=0;
		floppy_readTrackMachine_init();
		verifyMode=1;

		mfm_read_setEnableState(ENABLE);
		abortVerify=0;
		while (sectorsRead < geometry_sectors && !abortVerify)
		{

			if(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
			{
				//printf("TO\n");
				setupStepTimer(10000);
				failCnt++;
				if (failCnt > 4)
				{
					printf("Failed to verify Track:");
					for (i=0;i<geometry_sectors;i++)
					{
						if (trackSectorRead[i])
						{
							printf("K");
						}
						else
						{
							printf("_");
						}
						trackSectorRead[i]=0;
					}
					printf("\n");
					abortVerify=1;
				}
			}

			int readTrackMachineRet;

			if (mfm_mode == MFM_MODE_AMIGA)
				readTrackMachineRet=floppy_amiga_readTrackMachine(cylinder,head);
			else
				readTrackMachineRet=floppy_iso_readTrackMachine(cylinder,head);

			if (readTrackMachineRet)
			{
				printf("readTrackMachineRet:%d\n",readTrackMachineRet);
				abortVerify=1;
			}
		}
		mfm_read_setEnableState(DISABLE);

		if (sectorsRead == geometry_sectors)
			return 0;
	}

	return 1;
}


#define RAWBLOCK_TYPE_HEAD 1
#define RAWBLOCK_TYPE_HAS_VARIABLE_DENSITY 2
#define RAWBLOCK_TYPE_TIME_DATA 4

int raw_cellLengthDecrement=0;

int floppy_writeRawTrack(int cylinder, int head)
{
	int i;
	uint8_t *trackData=NULL;
	uint8_t *timeData=NULL;


	int trackDataSize=0;
	int timeDataSize=0;

	unsigned int timeDataCellLength=0;
	unsigned int timeDataCellReloadPos=0;
	int timeDataUsed=0;

	uint8_t *cylBufPtr=trackBuffer;
	while ( (timeDataUsed && (!trackData || !timeData)) || (!timeDataUsed && !trackData) )
	{

		int blocklen=((int)cylBufPtr[0]<<8) | cylBufPtr[1];

		if (blocklen==0)
		{
			printf("Konnte nicht alle notwendigen Daten finden!\n");
			return 1;
		}

		int blocktype=cylBufPtr[2];

		//printf("Found block %d %d\n",blocklen,cylBufPtr[2]);

		if ((blocktype & RAWBLOCK_TYPE_HEAD) == head)
		{
			if (blocktype & RAWBLOCK_TYPE_HAS_VARIABLE_DENSITY)
				timeDataUsed=1;

			if (blocktype & RAWBLOCK_TYPE_TIME_DATA)
			{
				timeData=cylBufPtr+3;
				timeDataSize=blocklen;
			}
			else
			{
				trackData=cylBufPtr+3;
				trackDataSize=blocklen;
			}
		}

		cylBufPtr+=3+blocklen;

	}


	assert (trackData >= &trackBuffer[0]);
	assert (trackData + trackDataSize < &trackBuffer[CYLINDER_BUFFER_SIZE]);

	printf("floppy_writeRawTrack %d %d %d %d %d\n",cylinder,head,trackDataSize,timeDataSize,timeDataUsed);

	mfm_mode=MFM_MODE_ISO;

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
		timeDataCellLength=MFM_BITTIME_DD*timeDataCellLength/2000;

		mfm_configureWrite(MFM_RAW,8);
		mfm_configureWriteCellLength(timeDataCellLength);
	}
	else
	{
		mfm_configureWrite(MFM_RAW,8);
		mfm_configureWriteCellLength(0);
	}

	mfm_read_setEnableState(DISABLE);
	mfm_write_setEnableState(ENABLE);

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
		assert(!(GPIOB->IDR & GPIO_Pin_5));

		//Ist das wirklich notwendig? Wir warten auf den Index und löschen die ganze Spur zur Sicherheit einmal...
		if (floppy_waitForIndex())
			return 1;

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
					timeDataCellLength=((int)timeData[0]<<8)|(int)timeData[1];
					timeDataCellReloadPos=((int)timeData[2]<<8)|(int)timeData[3];
					timeData+=4;
					timeDataCellLength=MFM_BITTIME_DD*timeDataCellLength/2000;

					mfm_configureWriteCellLength(timeDataCellLength);

				}

				if (!timeDataUsed)
				{
					if (raw_cellLengthDecrement&1)
					{
						if (i&1)
							mfm_configureWriteCellLength(mfm_decodeCellLength - (raw_cellLengthDecrement>>1) -1);
						else
							mfm_configureWriteCellLength(mfm_decodeCellLength - (raw_cellLengthDecrement>>1));
					}
					else
						mfm_configureWriteCellLength(mfm_decodeCellLength - (raw_cellLengthDecrement>>1));
				}

				assert(trackData[i]);
				mfm_blockedWrite(trackData[i]);

				if (!indexHappened)
					assert(overflownBytes==0);

				if (indexHappened)
					overflownBytes++;

				assert(!(GPIOB->IDR & GPIO_Pin_5));

				if (head)
					assert(!(GPIOB->IDR & GPIO_Pin_11));
				else
					assert((GPIOB->IDR & GPIO_Pin_11));

				assert (trackData >= &trackBuffer[0]);
				assert (trackData < &trackBuffer[CYLINDER_BUFFER_SIZE]);
			}
		}

		mfm_blockedWrite(0x55);

		assert(!(GPIOB->IDR & GPIO_Pin_5));
		floppy_setWriteGate(0);
		assert((GPIOB->IDR & GPIO_Pin_5));

		//Wir hängen noch ein paar Bytes ran, damit ein GAP entstehen kann.

		for (i=0; i < 35; i++)
		{
			mfm_blockedWrite(0x55);

			if (indexHappened)
				overflownBytes++;
		}


		if (overflownBytes)
		{
			if (timeDataUsed)
			{
				printf("track overflown with %d and timeData.. ill accept...\n",overflownBytes);
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

	mfm_write_setEnableState(DISABLE);

	return 0;
}

int floppy_writeAndVerifyCylinder(unsigned int cylinder)
{
	int head=0;

	floppy_stepToCylinder(cylinder);

	//Nach dem steppen warten wir einmal zusätzlich auf den Index.
	floppy_waitForIndex();

	for (head=0; head < geometry_heads; head++)
	{
		floppy_setHead(head);

		if (geometry_format == FLOPPY_FORMAT_RAW)
		{
			if (floppy_writeRawTrack(cylinder,head))
				return 1;
		}
		else
		{
			if (floppy_writeAndVerifyTrack(cylinder,head))
				return 1;
		}
	}
	//printf("Written and verified Cylinder!\n");

	return 0;
}

int floppy_readCylinder(unsigned int cylinder)
{

	unsigned int i=0;
	int head=0;
	int failCnt=0;

	floppy_stepToCylinder(cylinder);

	//printf("Stepped to track %d",track);

	mfm_read_setEnableState(ENABLE);

	for (head=0; head < geometry_heads; head++)
	{
		floppy_setHead(head);

		setupStepTimer(10000);

		failCnt=0;
		floppy_readTrackMachine_init();
		while (sectorsRead < geometry_sectors)
		{

			if(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
			{
				//printf("TO\n");
				setupStepTimer(10000);
				failCnt++;
				if (failCnt > 4)
				{
					printf("Failed to read Track:");
					for (i=0;i<geometry_sectors;i++)
					{
						if (trackSectorRead[i])
						{
							printf("K");
						}
						else
						{
							printf("_");
						}
						trackSectorRead[i]=0;
					}
					printf("\n");
					return 1;
				}
			}

			if (mfm_mode == MFM_MODE_AMIGA)
				floppy_amiga_readTrackMachine(cylinder,head);
			else
				floppy_iso_readTrackMachine(cylinder,head);
		}
	}

	mfm_read_setEnableState(DISABLE);

	return 0;
}

void floppy_debugTrackDataMachine(int track, int head )
{
	printf("debug %d %d\n",track,head);

	floppy_stepToCylinder(track);
	floppy_setHead(head);
	floppy_configureFormat(FLOPPY_FORMAT_AMIGA_DD,0,0,0);
	mfm_read_setEnableState(ENABLE);

	setupStepTimer(10000);

	int failCnt=0;
	floppy_readTrackMachine_init();

	while (failCnt < 4)
	{

		if(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
		{
			//printf("TO\n");
			setupStepTimer(10000);
			failCnt++;
		}

		floppy_amiga_readTrackMachine(track,head);
	}

	mfm_read_setEnableState(DISABLE);
}

