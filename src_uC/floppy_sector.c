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
#include "floppy_flux_read.h"
#include "floppy_flux_write.h"
#include "floppy_flux.h"

//arm-none-eabi-cpp floppy_sector.c -I CMSIS/ -IUtilities/ -I STM32F4xx_StdPeriph_Driver/inc -I pt-1.4/

//unsigned char trackBuffer[SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER];

uint32_t cylinderBuffer[CYLINDER_BUFFER_SIZE / 4];
uint32_t cylinderSize=0;

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
	"C64",
	"Raw MFM"
};

enum floppyFormat floppy_discoverFloppyFormat(int cylinder, int head)
{
	int failCnt;
	enum floppyFormat flopfrmt;


	floppy_stepToCylinder(cylinder);
	floppy_setHead(head);
	flux_read_setEnableState(ENABLE);

	//for (flopfrmt=FLOPPY_FORMAT_ISO_DD; flopfrmt <= FLOPPY_FORMAT_C64; flopfrmt++)
	flopfrmt=FLOPPY_FORMAT_C64;
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

			if (flux_mode==FLUX_MODE_MFM_AMIGA)
				floppy_amiga_readTrackMachine(cylinder,head);
			else if (flux_mode==FLUX_MODE_MFM_ISO)
				floppy_iso_readTrackMachine(cylinder, head);
			else
				floppy_c64_readTrackMachine(cylinder);
		}

		printf("Aborted with results: %d %d 0x%x\n",sectorsDetected,flopfrmt,lastSectorDataFormat);

		if (sectorsDetected >= 5 && sectorsRead >= 3)
		{
			printf("Format: %s\n",formatStr[flopfrmt]);
			return flopfrmt;
		}

	}

	flux_read_setEnableState(DISABLE);

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

	floppy_readErrorHappened=0;
	trackReadState=0;
	lastSectorDataFormat=0;
	verifyMode=0;
}


void printSectorReadState()
{
	int i;
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
}

int floppy_writeAndVerifyTrack(int cylinder, int head)
{
	int try=0;
	int failCnt=0;
	unsigned int abortVerify=0;

	for (try=0; try < 5; try++)
	{
		//printf("Write... %d %d\n",cylinder,head);
#if 1
		flux_write_setEnableState(ENABLE);
		//printf("mfm_write enabled\n");

		if (geometry_format & FLOPPY_FORMAT_RAW)
		{
			if (floppy_raw_writeTrack(cylinder,head))
				return 2;
		}
		else if (flux_mode == FLUX_MODE_MFM_AMIGA)
		{
			if (floppy_amiga_writeTrack(cylinder,head))
				return 2;
		}
		else if (flux_mode == FLUX_MODE_MFM_ISO)
		{
			if (floppy_iso_writeTrack(cylinder,head))
				return 2;
		}
		else
		{
			if (floppy_c64_writeTrack(cylinder))
				return 2;
		}

		flux_write_setEnableState(DISABLE);
#endif
		printf("Verify... %d %d %d\n", (int)geometry_format, (int)flux_mode, (int)geometry_sectors);
		setupStepTimer(10000);

		failCnt=0;
		floppy_readTrackMachine_init();
		//printSectorReadState();

		if (geometry_format == FLOPPY_FORMAT_C64)
			floppy_c64_setTrackSettings(floppy_c64_trackToExpect(cylinder));

		verifyMode=1;

		flux_read_setEnableState(ENABLE);
		abortVerify=0;
		while (sectorsRead < geometry_sectors && !abortVerify)
		{
			int readTrackMachineRet;

			if (geometry_format & FLOPPY_FORMAT_RAW)
				readTrackMachineRet=floppy_raw_readTrackMachine(cylinder,head);
			else if (flux_mode == FLUX_MODE_MFM_AMIGA)
				readTrackMachineRet=floppy_amiga_readTrackMachine(cylinder,head);
			else if (flux_mode == FLUX_MODE_MFM_ISO)
				readTrackMachineRet=floppy_iso_readTrackMachine(cylinder,head);
			else
				readTrackMachineRet=floppy_c64_readTrackMachine(cylinder);


			if (readTrackMachineRet)
			{
				printf("readTrackMachineRet:%d\n",readTrackMachineRet);
				abortVerify=1;
			}

			if(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
			{
				//printf("TO\n");
				setupStepTimer(10000);
				failCnt++;
				if (failCnt > 4)
				{
					printf("Failed to verify Track:");
					printSectorReadState();
					abortVerify=1;
				}
			}

		}
		flux_read_setEnableState(DISABLE);

		if (sectorsRead == geometry_sectors)
		{
			//printf("Finished verify\n");
			//printSectorReadState();
			return 0;
		}
	}

	return 1;
}



int floppy_writeAndVerifyCylinder(unsigned int cylinder)
{
	int head=0;

	floppy_stepToCylinder(cylinder);

	if (geometry_format == FLOPPY_FORMAT_ISO_DD || geometry_format == FLOPPY_FORMAT_ISO_HD)
	{
		int i;
		uint8_t *cylinderBufu8=(uint8_t*)cylinderBuffer;
		for(i=0;i<geometry_sectors;i++)
		{
			geometry_iso_sectorId[i]=*cylinderBufu8;
			cylinderBufu8++;

		}

		for(i=0;i<geometry_sectors;i++)
		{
			geometry_iso_sectorHeaderSize[i]=*cylinderBufu8 & 0x0f;
			geometry_iso_sectorErased[i]=(*cylinderBufu8 & 0x80) ? 1 : 0;
			cylinderBufu8++;
		}

		for(i=0;i<geometry_sectors;i++)
		{
			geometry_actualSectorSize[i]=(((unsigned short)cylinderBufu8[0])<<8) | (unsigned short)cylinderBufu8[1];
			cylinderBufu8+=2;
		}


		printf("floppy_writeAndVerifyCylinder with ISO custom settings:\n");
		for(i=0;i<geometry_sectors;i++)
		{
			printf("%d %x %d %d %d\n",
					i,
					geometry_iso_sectorId[i],
					geometry_iso_sectorHeaderSize[i],
					geometry_iso_sectorErased[i],
					geometry_actualSectorSize[i]);
		}

	}

	//Nach dem steppen warten wir einmal zusätzlich auf den Index.
	floppy_waitForIndex();

	for (head=0; head < geometry_heads; head++)
	{
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

	flux_read_setEnableState(ENABLE);

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

			if (flux_mode == FLUX_MODE_MFM_AMIGA)
				floppy_amiga_readTrackMachine(cylinder,head);
			else if (flux_mode == FLUX_MODE_MFM_ISO)
				floppy_iso_readTrackMachine(cylinder,head);
			else
				floppy_c64_readTrackMachine(cylinder);
		}
	}

	flux_read_setEnableState(DISABLE);

	return 0;
}


int floppy_polarizeCylinder(unsigned int cylinder)
{
	int head;

	floppy_stepToCylinder(cylinder);

	if (floppy_waitForIndex())
		return 1;

	for (head=0; head < geometry_heads; head++)
	{
		floppy_setHead(head);
		floppy_setWriteGate(1);

		if (floppy_waitForIndex())
			return 1;
		floppy_setWriteGate(0);
	}

	return 0;
}

void floppy_debugTrackDataMachine(int track, int head )
{
	printf("debug %d %d\n",track,head);

	floppy_stepToCylinder(track);
	floppy_setHead(head);
	//floppy_configureFormat(FLOPPY_FORMAT_AMIGA_DD,0,0);
	//floppy_configureFormat(FLOPPY_FORMAT_ISO_DD,0,0);
	floppy_configureFormat(FLOPPY_FORMAT_C64,0,0,0);

	floppy_c64_setTrackSettings(0);

	flux_read_setEnableState(ENABLE);

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

		//floppy_amiga_readTrackMachine(track,head);
		//floppy_iso_readTrackMachine(track,head);
		floppy_c64_readTrackMachine(track);
	}

	flux_read_setEnableState(DISABLE);
}

