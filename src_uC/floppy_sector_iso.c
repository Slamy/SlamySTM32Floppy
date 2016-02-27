/*
 * floppy_sector_iso.c
 *
 *  Created on: 23.02.2016
 *      Author: andre
 */

#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_crc.h"
#include "floppy_mfm.h"
#include "floppy_sector.h"
#include "floppy_control.h"
#include "floppy_settings.h"

#define DATA_AFTER_4E 2

uint8_t floppy_iso_sectorInterleave[18];

void floppy_iso_buildSectorInterleavingLut()
{
	int i;
	int sector=1;
	int placePos=0;

	for (i=0;i<sizeof(floppy_iso_sectorInterleave);i++)
		floppy_iso_sectorInterleave[i]=0;

	while (sector <= geometry_sectors)
	{
		floppy_iso_sectorInterleave[placePos]=sector;
		placePos+=(geometry_iso_sectorInterleave+1);
		if (placePos >= geometry_sectors)
		{
			placePos-=geometry_sectors;
		}

		while (floppy_iso_sectorInterleave[placePos])
			placePos++;

		sector++;
	}
}

int floppy_iso_getSectorNum(int sectorPos)
{
	//return ((sectorPos-1)*(geometry_iso_sectorInterleave+1) % geometry_sectors) +1;
	return floppy_iso_sectorInterleave[sectorPos-1];
}


void floppy_iso_evaluateSectorInterleaving()
{
	int sectorpos=0;
	int expectedSector=1;

	/*
	for (sectorpos=1; sectorpos <= geometry_sectors ; sectorpos++)
	{
		printf("%2d ",sectorpos);
	}
	printf("\n");


	for (sectorpos=1; sectorpos <= geometry_sectors ; sectorpos++)
	{
		printf("%2d ",floppy_iso_getSectorNum(sectorpos));
	}
	printf("\n\n");
	*/

	while (expectedSector <= geometry_sectors)
	{
		for (sectorpos=1; sectorpos <= geometry_sectors; sectorpos++)
		{
			if (expectedSector==floppy_iso_getSectorNum(sectorpos))
			{
				printf("%2d ",floppy_iso_getSectorNum(sectorpos));
				expectedSector++;
			}
			else
			{
				printf("-- ");
			}
		}
		printf("\n");
	}
}


void floppy_iso_writeTrack(int cylinder, int head, int simulate)
{
	static uint8_t *sectorData;

	int i;
	int sector,sectorPos;

	//Ist das wirklich notwendig? Wir warten auf den Index und löschen die ganze Spur zur Sicherheit einmal...
	floppy_waitForIndex();
	if (!simulate)
	{
		floppy_setWriteGate(1);
		floppy_waitForIndex();
	}

	//printf("Index!\n");

	mfm_configureWrite(0,8);

	if (geometry_iso_trackstart_00)
	{
		//Nun der Track Header, wenn überhaupt erwünscht.
		for (i=0;i<geometry_iso_trackstart_4e;i++)
			mfm_blockedWrite(0x4E);

		for (i=0;i<geometry_iso_trackstart_00;i++)
			mfm_blockedWrite(0x00);

		mfm_configureWrite(1,16);
		for (i=0;i<3;i++)
			mfm_blockedWrite(0x5524);
		mfm_configureWrite(0,8);

		mfm_blockedWrite(0xFC);
	}


	//printf("Sektors %d\n",geometry_sectors);
	for (sectorPos=1; sectorPos <= geometry_sectors; sectorPos++)
	{
		sector=floppy_iso_getSectorNum(sectorPos);

		//printf("Write Sektor %d\n",sector);

		//Jetzt die einzelnen Sektoren. Erst der Header nach einem Gap
		for (i=0;i<geometry_iso_before_idam_4e;i++)
			mfm_blockedWrite(0x4E);

		for (i=0;i<geometry_iso_before_idam_00;i++)
			mfm_blockedWrite(0x00);

		crc=0xffff;

		mfm_configureWrite(1,16);
		for (i=0;i<3;i++)
		{
			mfm_blockedWrite(0x4489);
			crc_shiftByte(0xa1);
		}
		mfm_configureWrite(0,8);


		mfm_blockedWrite(0xfe);
		crc_shiftByte(0xfe);

		mfm_blockedWrite(cylinder);
		crc_shiftByte(cylinder);

		mfm_blockedWrite(head);
		crc_shiftByte(head);

		mfm_blockedWrite(sector);
		crc_shiftByte(sector);

		mfm_blockedWrite(2);
		crc_shiftByte(2);

		mfm_blockedWrite(crc>>8);
		mfm_blockedWrite(crc&0xff);

		//Das war der Header. Jetzt wieder ein Gap und dann die Daten.
		crc=0xffff;

		for (i=0;i<geometry_iso_before_data_4e;i++)
			mfm_blockedWrite(0x4E);

		for (i=0;i<geometry_iso_before_data_00;i++)
			mfm_blockedWrite(0x00);

		mfm_configureWrite(1,16);
		for (i=0;i<3;i++)
		{
			mfm_blockedWrite(0x4489);
			crc_shiftByte(0xa1);
		}
		mfm_configureWrite(0,8);

		sectorData=&((uint8_t*)trackBuffer)[((sector-1)+(head * geometry_sectors)) * geometry_payloadBytesPerSector];
		//printf("%x\n",sectorData[0]);

		mfm_blockedWrite(0xfb);
		crc_shiftByte(0xfb);

		//crc=0xFFFF;
		for (i=0;i<512;i++)
		{
			mfm_blockedWrite(sectorData[i]);
			crc_shiftByte(sectorData[i]);
		}

		mfm_blockedWrite(crc>>8);
		mfm_blockedWrite(crc&0xff);
	}

	for (i=0;i<DATA_AFTER_4E;i++)
		mfm_blockedWrite(0x4E);

	floppy_setWriteGate(0);
}

int floppy_iso_readTrackMachine(int expectedCyl, int expectedHead)
{
	static uint8_t *sectorData;

	static unsigned int header_cyl=0;
	static unsigned int header_head=0;
	static unsigned int header_sec=0;

	static unsigned int i=0;

	if (mfm_errorHappened)
	{
		//printf("Reset statemachine\n");
		mfm_errorHappened=0;
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
		mfm_blockedWaitForSyncWord(1);
		//printf("IDAM1\n");
		trackReadState++;
		break;
	case 2:
		//Wir warten auf das zweite Sync Word.
		mfm_blockedWaitForSyncWord(2);
		//printf("a%d\n",mfm_inSync);
		trackReadState++;
		break;

	case 3:
		//Eigentlich hätte man hier auch die Amiga Routinen laufen lassen können. Aber es ist einfach zu komplex, alles in einem zu lassen.
		mfm_blockedWaitForSyncWord(3);
		trackReadState++;
		break;
	case 4:
		//Es muss ein Iso Format sein. Aber ist es ein Header oder Daten?
		mfm_blockedRead();

		switch (mfm_decodedByte)
		{
			case 0xfe:
				crc_shiftByte(mfm_decodedByte);
				trackReadState=10; //ISO IDAM - Sector Header
				break;
			case 0xfb:
				crc_shiftByte(mfm_decodedByte);
				trackReadState=20; //ISO DAM - Sector Data
				break;
			case 0xa1:
				printf("a%x\n",mfm_decodedByte);
				break;
			default:
				printf("u%x\n",mfm_decodedByte);
				trackReadState=0;
		}
		break;

	case 10: //IDAM - Sector Header
		mfm_blockedRead();
		header_cyl=mfm_decodedByte;
		crc_shiftByte(header_cyl);

		mfm_blockedRead();
		header_head=mfm_decodedByte;
		crc_shiftByte(header_head);

		mfm_blockedRead();
		header_sec=mfm_decodedByte;
		crc_shiftByte(header_sec);

		mfm_blockedRead();
		if (mfm_decodedByte!=2)
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
		mfm_blockedRead();
		crc_shiftByte(mfm_decodedByte);
		mfm_blockedRead();
		crc_shiftByte(mfm_decodedByte);

		if (crc != 0) //crc has to be 0 at the end for a correct result
		{
			printf("**** idam crc error %d %d %d\n",header_cyl,header_head,header_sec);
			header_cyl=0;
			header_head=0;
			header_sec=0;
		}
		else
		{
			printf("SecHead: %d %d %d\n",header_cyl,header_head,header_sec);

			if (header_cyl!=expectedCyl)
			{
				header_cyl=0;
				header_head=0;
				header_sec=0;
				printf("Cylinder is wrong!\n");
				return 1;
			}

			if (header_head != expectedHead)
			{
				header_cyl=0;
				header_head=0;
				header_sec=0;
				printf("Head is wrong!\n");
				return 2;
			}

			if (header_sec > geometry_sectors)
			{
				printf("Ignore Sector\n");
				header_cyl=0;
				header_head=0;
				header_sec=0;
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
			//printf("sectorData calc %d %d %d %d\n",header_sec,expectedHead,geometry_sectors,geometry_payloadBytesPerSector);

			sectorData=&((uint8_t*)trackBuffer)[((header_sec-1)+(expectedHead * geometry_sectors)) * geometry_payloadBytesPerSector];
		}
		break;
	case 21:
		mfm_blockedRead();

		if (verifyMode && sectorData[i]!=mfm_decodedByte)
		{
			return 3; //verify failed
		}
		else
			sectorData[i]=mfm_decodedByte;

		crc_shiftByte(sectorData[i]);
		i++;

		if (i==512)
			trackReadState++;
		break;
	case 22:
		//Datenblock endet mit CRC
		mfm_blockedRead();
		crc_shiftByte(mfm_decodedByte);
		mfm_blockedRead();
		crc_shiftByte(mfm_decodedByte);

		if (crc != 0) //crc has to be 0 at the end for a correct result
		{
			printf("**** dam crc error %d %d %d\n",header_cyl,header_head,header_sec);
		}
		else
		{
			//printf("SecDat: %d %d %d %x\n",header_cyl,header_head,header_sec,sectorData[0]);

			if (!trackSectorRead[(header_sec-1)+(expectedHead * geometry_sectors)])
			{
				sectorsRead++;
				trackSectorRead[(header_sec-1)+(expectedHead * geometry_sectors)]=1;
			}

			lastSectorDataFormat=0xfb;

		}
		trackReadState=0;

		break;



	default:
		trackReadState=0;
	}

	return 0;
}
