#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_crc.h"
#include "floppy_mfm.h"
#include "floppy_sector.h"

#define BLOCKED_READ_MFM_OR_RESTART(pt,val) \
	PT_WAIT_WHILE(pt,mfm_decodingStatus == SYNC3); if (mfm_decodingStatus != DATA_VALID) {PT_RESTART(pt);}; mfm_decodingStatus=SYNC3; val=mfm_decodedByte;

#define WAIT_FOR_IDAM(pt) \
	PT_WAIT_UNTIL(pt,mfm_decodingStatus >= SYNC3);


//arm-none-eabi-cpp floppy_sector.c -I CMSIS/ -IUtilities/ -I STM32F4xx_StdPeriph_Driver/inc -I pt-1.4/

unsigned char trackBuffer[SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER];
unsigned int sectorsRead=0;
unsigned char trackSectorRead[MAX_SECTORS_PER_CYLINDER];

unsigned char lastSectorDataFormat=0;

unsigned int timeOut=0;
unsigned char errorHappened=0;


void waitForSyncWord(int expectNum)
{
	timeOut=100000;

	while (mfm_inSync!=expectNum && timeOut)
		timeOut--;

	if (mfm_inSync!=expectNum)
		errorHappened=1;
}


unsigned char blockedReadMFM()
{
	timeOut=1000;

	while (!mfm_decodedByteValid && timeOut)
		timeOut--;

	if (mfm_decodedByteValid)
	{
		mfm_decodedByteValid=0;
		return mfm_decodedByte;
	}

	errorHappened=1;
	return 0;
}

char *formatStr[]=
{
	"UNKNOWN",
	"Iso DD, 9 Sektoren",
	"Iso HD, 18 Sektoren",
	"Amiga DD, 11 Sektoren"
};

enum mfmMode floppy_discoverFloppyFormat()
{
	int trackReadState=0;
	int highestSectorNum=0;
	enum mfmMode mfmMode;
	int failCnt;
	enum mfmMode flopfrmt=UNKNOWN;

	//Wir versuchen es zuerst mit HD, dann mit DD
	floppy_stepToTrack00();
	//floppy_stepToTrack(2);
	floppy_setHead(0);
	mfm_setEnableState(ENABLE);

	for (mfmMode=MFM_ISO_DD; mfmMode <= MFM_AMIGA_DD; mfmMode++)
	{
		mfm_setDecodingMode(mfmMode);

		setupStepTimer(10000);

		failCnt=0;
		floppy_trackDataMachine_init();

		while (failCnt < 4)
		{

			if(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
			{
				//printf("TO\n");
				setupStepTimer(10000);
				failCnt++;
			}

			floppy_iso_trackDataMachine(0,0);
		}

		printf("Aborted with results: %d %d 0x%x\n",sectorsRead,mfmMode,lastSectorDataFormat);

		if (sectorsRead==18 && mfmMode==MFM_ISO_HD && lastSectorDataFormat==0xfb)
		{
			flopfrmt=MFM_ISO_HD;
			printf("Format: %s\n",formatStr[flopfrmt]);
		}
		else if (sectorsRead==9 && mfmMode==MFM_ISO_DD && lastSectorDataFormat==0xfb)
		{
			flopfrmt=MFM_ISO_DD;
			printf("Format: %s\n",formatStr[flopfrmt]);
		}
		else if (sectorsRead==11 && mfmMode==MFM_AMIGA_DD && lastSectorDataFormat==0xff)
		{
			flopfrmt=MFM_AMIGA_DD;
			printf("Format: %s\n",formatStr[flopfrmt]);
		}
		else
		{
			printf("Format nicht erkennbar!\n");
		}



	}

	return flopfrmt;

}

static unsigned int trackReadState=0;

void floppy_trackDataMachine_init()
{
	int i;
	sectorsRead=0;
	for (i=0;i<MAX_SECTORS_PER_CYLINDER;i++)
		trackSectorRead[i]=0;

	errorHappened=0;
	trackReadState=0;
	lastSectorDataFormat=0;
}




int floppy_iso_trackDataMachine(int expectedTrack, int expectedHead)
{
	static unsigned char *sectorData;

	//Für Iso
	static unsigned int header_cyl=0;
	static unsigned int header_head=0;
	static unsigned int header_sec=0;

	//Für Amiga
	static unsigned int header_track=0;
	static unsigned int header_secRem=0;

	static unsigned short amiga_rawMfm[2+8+2]; //4 byte header + 16 byte os info + 4 byte checksum
	static unsigned short amiga_rawMfm_unshifted[4+16+4];
	static unsigned short amiga_checksum_even=0;
	static unsigned short amiga_checksum_odd=0;

	static unsigned int temp=0;
	static unsigned int i=0;

	if (errorHappened)
	{
		//printf("Reset statemachine\n");
		errorHappened=0;
		trackReadState=0;
	}

	switch (trackReadState)
	{
	case 0:
		crc=0xFFFF; //reset crc
		crc_shiftByte(0xa1);
		crc_shiftByte(0xa1);
		crc_shiftByte(0xa1);

		trackReadState++;
		break;
	case 1:
		//printf("**** Wait for IDAM\n");
		mfm_inSync=0;
		mfm_decodedByteValid=0;

		//Wir warten auf das erste Sync Word
		waitForSyncWord(1);
		//printf("IDAM1\n");
		trackReadState++;
		break;
	case 2:
		//Wir warten auf das zweite Sync Word.
		//Sowohl beim ISO Format, als auch beim Amiga existiert dies.
		waitForSyncWord(2);
		//printf("a%d\n",mfm_inSync);
		trackReadState++;
		break;

	case 3:
		//Jetzt wird es schwierig. Das ISO Format benötigt in jedem Fall noch ein Sync Word
		//Der Amiga beginnt allerdings schon mit der Header ID.

		//Wir invalidieren das Sync Word und lesen Daten.
		mfm_decodedByteValid=0;
		temp=blockedReadMFM();

		if (mfm_inSync==3)
		{
			//Noch ein Sync Word? Es ist also höchstwahrscheinlich ISO
			trackReadState++;
			//printf("S\n");
		}
		else if ((temp & 0xf0)==0xf0) //Laut Doku eigentlich 0xff. Aber das liegt an dem ungewöhnlichen Even und Odd Encoding.
		{
			//Amiga Sector Format wahrscheinlich.... ach was ein Scheiß
			trackReadState=30; //Amiga 1.0 - Sector
			amiga_rawMfm[0]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1; //Odd Byte
			amiga_rawMfm_unshifted[0]=mfm_savedRawWord & AMIGA_MFM_MASK;

			amiga_checksum_even=0;
			amiga_checksum_odd=mfm_savedRawWord & AMIGA_MFM_MASK;

			i=1;
			//printf("A%x\n",mfm_savedRawWord);
		}
		else
		{
			trackReadState=0;
			printf("u%x\n",temp);
		}

		break;
	case 4:
		//Es muss ein Iso Format sein. Aber ist es ein Header oder Daten?
		temp=blockedReadMFM();

		switch (temp)
		{
			case 0xfe:
				crc_shiftByte(temp);
				trackReadState=10; //ISO IDAM - Sector Header
				break;
			case 0xfb:
				crc_shiftByte(temp);
				trackReadState=20; //ISO DAM - Sector Data
				break;
			default:
				printf("u%x\n",temp);
				trackReadState=0;
		}
		break;

	case 10: //IDAM - Sector Header
		header_cyl=blockedReadMFM();
		crc_shiftByte(header_cyl);

		header_head=blockedReadMFM();
		crc_shiftByte(header_head);

		header_sec=blockedReadMFM();
		crc_shiftByte(header_sec);

		if (blockedReadMFM()!=2)
		{
			trackReadState=0;
		}
		else
		{
			crc_shiftByte(2);
			trackReadState++;
		}
		break;
	case 11:

		crc_shiftByte(blockedReadMFM());
		crc_shiftByte(blockedReadMFM());

		if (crc != 0) //crc has to be 0 at the end for a correct result
		{
			printf("**** idam crc error %d %d %d\n",header_cyl,header_head,header_sec);
			header_cyl=0;
			header_head=0;
			header_sec=0;
		}
		else
		{
			//printf("SecHead: %d %d %d\n",header_cyl,header_head,header_sec);

			if (header_cyl!=expectedTrack)
			{
				printf("Track is wrong!\n");
				return 2;
			}

			if (header_head != expectedHead)
			{
				header_cyl=0;
				header_head=0;
				header_sec=0;
				printf("Head is wrong!\n");
				return 3;
			}
		}

		trackReadState=0;

		break;

	case 20: //DAM - Sector Data

		if (header_sec==0)
			trackReadState=0; //Keine aktuellen Headerinfos. Also zurück zum Anfang!
		else
		{
			i=0;
			trackReadState++;
			sectorData=&trackBuffer[((header_sec-1)+(expectedHead*MAX_SECTORS_PER_TRACK))*SECTOR_SIZE];
		}
		break;
	case 21:
		sectorData[i]=blockedReadMFM();
		crc_shiftByte(sectorData[i]);
		i++;

		if (i==512)
			trackReadState++;
		break;
	case 22:
		//Datenblock endet mit CRC
		crc_shiftByte(blockedReadMFM());
		crc_shiftByte(blockedReadMFM());

		if (crc != 0) //crc has to be 0 at the end for a correct result
		{
			printf("**** dam crc error %d %d %d\n",header_cyl,header_head,header_sec);
		}
		else
		{
			//printf("SecDat: %d %d %d\n",header_cyl,header_head,header_sec);

			if (!trackSectorRead[(header_sec-1)+(expectedHead*MAX_SECTORS_PER_TRACK)])
			{
				sectorsRead++;
				trackSectorRead[(header_sec-1)+(expectedHead*MAX_SECTORS_PER_TRACK)]=1;
			}

			lastSectorDataFormat=0xfb;

		}
		trackReadState=0;

		break;



	case 30: //Amiga 1.0 - Sector - Verarbeite die restlichen 3 Byte des 4 Byte Blocks

		/*
		header_track=blockedReadMFM();
		amiga_rawMfm[1]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1; //Odd Byte

		header_sec=blockedReadMFM();
		amiga_rawMfm[0]|=mfm_savedRawWord & AMIGA_MFM_MASK; //Even Byte

		header_secRem=blockedReadMFM();
		amiga_rawMfm[1]|=mfm_savedRawWord & AMIGA_MFM_MASK; //Even Byte
		 */

		blockedReadMFM();

		if (i>=2)
		{
			amiga_rawMfm[i%2]|=(mfm_savedRawWord & AMIGA_MFM_MASK); //Even Byte
			amiga_checksum_even^=mfm_savedRawWord & AMIGA_MFM_MASK;
		}
		else
		{
			amiga_rawMfm[i%2]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1; //Odd Byte
			amiga_checksum_odd^=mfm_savedRawWord & AMIGA_MFM_MASK;
		}

		amiga_rawMfm_unshifted[i]=mfm_savedRawWord & AMIGA_MFM_MASK;
		i++;
		//printf("AmiSec %04x %04x\n",amiga_rawMfm[0],amiga_rawMfm[1]);
		if (i==4)
		{
			i=0;
			sectorsRead++;
			trackReadState++;
		}
		break;
	case 31:
		//16 byte block of OS recovery????? brauchen wa nicht. aber für checksumme wichtig
		blockedReadMFM();

		if (i>=8)
		{
			amiga_rawMfm[2+i%8]|=(mfm_savedRawWord & AMIGA_MFM_MASK); //Even Byte
			amiga_checksum_even^=mfm_savedRawWord & AMIGA_MFM_MASK;
		}
		else
		{
			amiga_rawMfm[2+i%8]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1; //Odd Byte
			amiga_checksum_odd^=mfm_savedRawWord & AMIGA_MFM_MASK;
		}

		amiga_rawMfm_unshifted[4+i]=mfm_savedRawWord & AMIGA_MFM_MASK;
		i++;
		if (i==16)
		{
			trackReadState++;
			i=0;
		}
		break;

	case 32:
		//4 Byte Block für Header Checksum
		blockedReadMFM();

		if (i>=2)
		{
			amiga_rawMfm[2+8+i%2]|=(mfm_savedRawWord & AMIGA_MFM_MASK); //Even Byte
			//amiga_checksum_even^=mfm_savedRawWord & AMIGA_MFM_MASK;
		}
		else
		{
			amiga_rawMfm[2+8+i%2]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1; //Odd Byte
			//amiga_checksum_odd^=mfm_savedRawWord & AMIGA_MFM_MASK;
		}

		amiga_rawMfm_unshifted[4+16+i]=mfm_savedRawWord & AMIGA_MFM_MASK;
		i++;
		if (i==4)
		{

			//trackReadState++;
			for (i=0;i < 2+8+2 ; i++)
				printf("%04x %04x %04x\n",amiga_rawMfm[i],amiga_rawMfm_unshifted[i<<1],amiga_rawMfm_unshifted[(i<<1)+1]);
			printf("\n");
			trackReadState=0;

			printf("%04x %04x\n",amiga_checksum_even,amiga_checksum_odd);
		}
		//printf("AmiSec2: %x %x %x\n",header_track,header_sec,header_secRem);

		break;

	default:
		trackReadState=0;
	}
}

int floppy_readCylinder(unsigned int track, unsigned int expectedSectors)
{

	unsigned int i=0;
	int head=0;
	int failCnt=0;

	floppy_stepToTrack(track);

	//printf("Stepped to track %d",track);

	for (head=0; head < 2; head++)
	{
		floppy_setHead(head);

		setupStepTimer(10000);

		failCnt=0;
		floppy_trackDataMachine_init();
		while (sectorsRead < expectedSectors)
		{

			if(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
			{
				//printf("TO\n");
				setupStepTimer(10000);
				failCnt++;
				if (failCnt > 4)
				{
					printf("Failed to read Track:");
					for (i=0;i<MAX_SECTORS_PER_CYLINDER;i++)
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

			floppy_iso_trackDataMachine(track,head);
		}
	}

	return 0;
}

void floppy_debugTrackDataMachine(int track, int head )
{
	printf("debug %d %d\n",track,head);

	floppy_stepToTrack(track);
	floppy_setHead(head);
	mfm_setDecodingMode(MFM_AMIGA_DD);
	mfm_setEnableState(ENABLE);

	setupStepTimer(10000);

	int failCnt=0;
	floppy_trackDataMachine_init();

	while (failCnt < 2)
	{

		if(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
		{
			//printf("TO\n");
			setupStepTimer(10000);
			failCnt++;
		}

		floppy_iso_trackDataMachine(track,head);
	}
}

