#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_crc.h"
#include "floppy_mfm.h"
#include "floppy_sector.h"
#include "floppy_control.h"
#include "floppy_settings.h"

//arm-none-eabi-cpp floppy_sector.c -I CMSIS/ -IUtilities/ -I STM32F4xx_StdPeriph_Driver/inc -I pt-1.4/

//unsigned char trackBuffer[SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER];
uint32_t trackBuffer[(MAX_SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER) / 4];

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
	"Amiga DD"
};

enum floppyFormat floppy_discoverFloppyFormat()
{
	int failCnt;
	enum floppyFormat flopfrmt;

	//Wir versuchen es zuerst mit HD, dann mit DD
	floppy_stepToCylinder00();
	//floppy_stepToTrack(2);
	floppy_setHead(0);
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
				floppy_amiga_readTrackMachine(0,0);
			else
				floppy_iso_readTrackMachine(0,0);
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

int floppy_writeAndVerifyCylinder(unsigned int cylinder)
{
	int head=0;

	floppy_stepToCylinder(cylinder);

	//Nach dem steppen warten wir einmal zusätzlich auf den Index.
	floppy_waitForIndex();

	for (head=0; head < geometry_heads; head++)
	{
		//floppy_setHead(head);
		floppy_setHead(head);

		if (floppy_writeAndVerifyTrack(cylinder,head))
			return 1;
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

