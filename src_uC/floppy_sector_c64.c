/*
 * floppy_sector_c64.c
 *
 *  Created on: 28.03.2016
 *      Author: andre
 */

#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_crc.h"
#include "floppy_gcr_read.h"
#include "floppy_sector.h"
#include "floppy_control.h"
#include "floppy_settings.h"
#include "floppy_flux_read.h"
#include "floppy_flux_write.h"
#include "assert.h"

extern const unsigned char gcrEncodeTable[];
extern const unsigned char gcrDecodeTable[];

void floppy_c64_blockedWriteByte(unsigned int byte)
{

	flux_blockedWrite((gcrEncodeTable[(byte>>4)&0xf] << 5) |
			gcrEncodeTable[byte&0xf]);

	//flux_blockedWrite(gcrEncodeTable[(byte>>4)&0xf]);
	//flux_blockedWrite(gcrEncodeTable[byte&0xf]);
}


int floppy_c64_writeTrack(uint32_t cylinder)
{
	uint8_t *sectorData;
	unsigned char sector;
	unsigned char id1=0x39;
	unsigned char id2=0x30;
	unsigned int i;
	unsigned char track=floppy_c64_trackToExpect(cylinder);

	printf("floppy_c64_writeTrack %d %d %d\n",(int)cylinder,(int)track,(int)geometry_payloadBytesPerSector);

	/*
	printf("Sektors %d -> ",geometry_sectors);
	for (sector=1; sector <= geometry_sectors; sector++)
		printf("%d ",floppy_iso_getSectorNum(sector));
	printf("\n");
	*/

	floppy_setWriteGate(1);

	if (floppy_waitForIndex())
		return 1;

	flux_configureWrite(FLUX_RAW,10);
	floppy_c64_setTrackSettings(track);
	flux_configureWriteCellLength(0);

	for (sector=0; sector < geometry_sectors; sector++)
	{
		//Header
		flux_configureWrite(FLUX_RAW,8);
		flux_blockedWrite(0xff);
		flux_blockedWrite(0xff);
		flux_blockedWrite(0xff);
		flux_blockedWrite(0xff);
		flux_blockedWrite(0xff);

		flux_configureWrite(FLUX_RAW,10);

		unsigned char checksum=sector^track^id1^id2;
		floppy_c64_blockedWriteByte(0x08);
		floppy_c64_blockedWriteByte(checksum);
		floppy_c64_blockedWriteByte(sector);
		floppy_c64_blockedWriteByte(track);
		floppy_c64_blockedWriteByte(id2);
		floppy_c64_blockedWriteByte(id1);
		floppy_c64_blockedWriteByte(0x0f);
		floppy_c64_blockedWriteByte(0x0f);

		//Gap #3
		flux_configureWrite(FLUX_RAW,8);
		flux_blockedWrite(0x55);
		flux_blockedWrite(0x55);
		flux_blockedWrite(0x55);
		flux_blockedWrite(0x55);
		flux_blockedWrite(0x55);

		flux_blockedWrite(0x55);
		flux_blockedWrite(0x55);
		flux_blockedWrite(0x55);
		flux_blockedWrite(0x55);

		//Data
		flux_blockedWrite(0xff);
		flux_blockedWrite(0xff);
		flux_blockedWrite(0xff);
		flux_blockedWrite(0xff);
		flux_blockedWrite(0xff);


		flux_configureWrite(FLUX_RAW,10);

		sectorData=(uint8_t*)&cylinderBuffer[sector * geometry_payloadBytesPerSector/4];
		floppy_c64_blockedWriteByte(0x07);
		checksum=0;

		for (i=0;i<geometry_payloadBytesPerSector;i++)
		{
			floppy_c64_blockedWriteByte(sectorData[i]);
			checksum^=sectorData[i];
		}
		floppy_c64_blockedWriteByte(checksum);

		floppy_c64_blockedWriteByte(0x00);
		floppy_c64_blockedWriteByte(0x00);

		flux_configureWrite(FLUX_RAW,8);

		//Gap #6
		for (i=0;i<geometry_c64_gap_size;i++)
			flux_blockedWrite(0x55);
	}

	//Spur auslaufen lassen...
	for (i=0;i<2;i++)
		flux_blockedWrite(0x55);

	flux_write_waitForUnderflow();

	floppy_setWriteGate(0);


	if (indexOverflowCount)
	{
		printf("index overflow occured: %d\n",indexOverflowCount);
		return 1;
	}
	printf("Write finished!\n");
	return 0;
}


int floppy_c64_readTrackMachine(int expectedCyl)
{
	static uint8_t *sectorData;

	static int header_sector=-1;
	static uint32_t header_track=0;
	static uint32_t header_id2=0;
	static uint32_t header_id1=0;
	static uint32_t header_checksum=0;

	static uint32_t i=0;

	if (floppy_readErrorHappened)
	{
		printf("R %d %d\n",trackReadState,(int)i);
		floppy_readErrorHappened=0;
		trackReadState=0;
	}

	switch (trackReadState)
	{
	case 0:
		gcr_blockedWaitForSyncState();
		trackReadState++;
		break;
	case 1:
		gcr_blockedRead();
		if (gcr_decodedByte==0x08) //Jeder Header fängt mit 0x08 an.
			trackReadState=10;
		else if(gcr_decodedByte==0x07) //Daten beginnen mit 0x07
			trackReadState=20;
		else
			floppy_readErrorHappened=1;
		break;
	case 10:
		gcr_blockedRead();
		header_checksum=gcr_decodedByte;

		gcr_blockedRead();
		header_checksum^=gcr_decodedByte;
		header_sector=gcr_decodedByte;

		gcr_blockedRead();
		header_checksum^=gcr_decodedByte;
		header_track=gcr_decodedByte;

		gcr_blockedRead();
		header_checksum^=gcr_decodedByte;
		header_id2=gcr_decodedByte;

		gcr_blockedRead();
		header_checksum^=gcr_decodedByte;
		header_id1=gcr_decodedByte;

		trackReadState++;
		break;
	case 11:
		if (header_checksum==0)
		{
			//printf("C64 SecHead %d %d %x %x\n",(int)header_track,(int)header_sector,(unsigned int)header_id1,(unsigned int)header_id2);

			if (!trackSectorDetected[header_sector])
			{
				sectorsDetected++;
				trackSectorDetected[header_sector]=1;
			}

			if (header_track!=floppy_c64_trackToExpect(expectedCyl))
			{
				header_track=0;
				header_sector=-1;
				printf("Cylinder is wrong!\n");
				floppy_readErrorHappened=1;
			}
			else if (header_sector >= geometry_sectors)
			{
				printf("Ignore Sector!\n");

				header_track=0;
				header_sector=-1;
			}

			trackReadState++;
		}
		else
		{
			printf("C64 wrong checksum\n");
			floppy_readErrorHappened=1;
		}
		break;
	case 12:

		i=1;
		gcr_blockedRead();
		if (gcr_decodedByte!=0x0f)
			floppy_readErrorHappened=1;

		i=2;
		gcr_blockedRead();
		if (gcr_decodedByte!=0x0f)
			floppy_readErrorHappened=1;

		trackReadState=0;
		break;


	case 20:
		//Nur Daten Block verarbeiten, wenn header_sector valide
		if (header_sector==-1)
			floppy_readErrorHappened=1;
		else
		{
			i=0;
			trackReadState++;
			sectorData=(uint8_t*)&cylinderBuffer[header_sector * geometry_payloadBytesPerSector/4];
			header_checksum=0;
		}

		break;
	case 21:
		gcr_blockedRead();
		uint8_t readBack=gcr_decodedByte;

		if (verifyMode)
		{

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
		flux_read_diffDebugFifoWrite(0x40000 | (readBack<<8) | sectorData[i]);
#endif

			if (sectorData[i]!=readBack)
			{
				STM_EVAL_LEDOn(LED4);

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO
				printDebugDiffFifo();
#endif
				printf("verify failed on sec %d  i=%d   bufpos %u %x != %x\n",
						header_sector,
						(int)i,
						(unsigned int)((uint32_t)&sectorData[i] - (uint32_t)cylinderBuffer),
						sectorData[i],
						(unsigned int)readBack);

				STM_EVAL_LEDOff(LED4);
				return 3; //verify failed
			}
		}
		else
			sectorData[i]=readBack;

		header_checksum^=readBack;
		i++;

		if (i==256)
			trackReadState++;
		break;
	case 22:
		//Die Checksumme
		gcr_blockedRead();
		header_checksum^=gcr_decodedByte;

		if (!header_checksum)
		{
			//set this sector as a verified / read out one
			if (!trackSectorRead[header_sector])
			{
				sectorsRead++;
				trackSectorRead[header_sector]=1;
				//printf("sector read %d\n",header_sector);
			}

		}
		else
		{
			printf("data block checksum error %d\n",header_sector);
		}

		if (floppy_readErrorHappened)
		{
			printf("Timeout on Checksum\n");
		}

		header_sector=-1;
		trackReadState=0;

		break;
	default:
		printf("Default state!\n");
		trackReadState=0;
		break;
	}


	return 0;
}

void floppy_c64_setTrackSettings(int trk)
{

	if (trk <= 17) //Track 1 - 17 -> Cylinder 0 - 16
	{
		flux_decodeCellLength=227; //307692 bit/s
		geometry_sectors=21;
		geometry_c64_gap_size=8;
	}
	else if (trk <= 24) //Track 18 - 24 -> Cylinder 17 - 23
	{
		flux_decodeCellLength=246; //285714 bit/s
		geometry_sectors=19;
		geometry_c64_gap_size=17;
	}
	else if (trk <= 30) //Track 25 - 30 -> Cylinder 24 - 29
	{
		flux_decodeCellLength=262; //266667 bit/s
		geometry_sectors=18;
		geometry_c64_gap_size=12;
	}
	else //Track 31 - Ende
	{
		flux_decodeCellLength=280; //250000 bit/s
		geometry_sectors=17;
		geometry_c64_gap_size=9;
	}
}

int floppy_c64_trackToExpect(int cylinder)
{
	//Cyl 0 -> Track 1
	//Cyl 1 -> Track 1
	//Cyl 2 -> Track 2
	return (cylinder>>1)+1;
}

void floppy_c64_stepToTrack(int track)
{
	//Track 1 findet sich by Cylinder 0.
	track--;

	//Auf geraden Spuren lässt es sich gut lesen. Auf ungeraden mit Lesefehlern.
	floppy_stepToCylinder((track<<1));
}


