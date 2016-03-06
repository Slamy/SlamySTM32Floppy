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

int floppy_iso_getSectorNum(int sectorPos)
{
	//return ((sectorPos-1)*(geometry_iso_sectorInterleave+1) % geometry_sectors) +1;
	return geometry_iso_sectorPos[sectorPos-1];
}


int floppy_iso_writeTrack(int cylinder, int head, int simulate)
{
	static uint8_t *sectorData;

	int i;
	int sector;
	int sectorPos;

	/*
	printf("Sektors %d -> ",geometry_sectors);
	for (sector=1; sector <= geometry_sectors; sector++)
		printf("%d ",floppy_iso_getSectorNum(sector));
	printf("\n");
	*/



	if (floppy_waitForIndex())
		return 1;

	if (!simulate)
	{
		floppy_setWriteGate(1);


		//Ist das wirklich notwendig? Wir warten auf den Index und löschen die ganze Spur zur Sicherheit einmal...
		/*
		if (floppy_waitForIndex())
			return 1;
		*/
	}

	//printf("Index!\n");

	mfm_configureWrite(MFM_ENCODE,8);

	if (geometry_iso_trackstart_00)
	{
		//Nun der Track Header, wenn überhaupt erwünscht.
		for (i=0;i<geometry_iso_trackstart_4e;i++)
			mfm_blockedWrite(0x4E);

		for (i=0;i<geometry_iso_trackstart_00;i++)
			mfm_blockedWrite(0x00);

		mfm_configureWrite(MFM_RAW,16);
		for (i=0;i<3;i++)
			mfm_blockedWrite(0x5524);
		mfm_configureWrite(MFM_ENCODE,8);

		mfm_blockedWrite(0xFC);
	}


	for (sectorPos=1; sectorPos <= geometry_sectors; sectorPos++)
	//for (sector=1; sector <= geometry_sectors; sector++)
	{
		sector=floppy_iso_getSectorNum(sectorPos);

		//printf("Write Sektor %d\n",sector);

		//Jetzt die einzelnen Sektoren. Erst der Header nach einem Gap
		for (i=0;i<geometry_iso_before_idam_4e;i++)
			mfm_blockedWrite(0x4E);

		for (i=0;i<geometry_iso_before_idam_00;i++)
			mfm_blockedWrite(0x00);

		crc=0xffff;

		mfm_configureWrite(MFM_RAW,16);
		for (i=0;i<3;i++)
		{
			mfm_blockedWrite(0x4489);
			crc_shiftByte(0xa1);
		}
		mfm_configureWrite(MFM_ENCODE,8);


		mfm_blockedWrite(0xfe);
		crc_shiftByte(0xfe);

		mfm_blockedWrite(cylinder);
		crc_shiftByte(cylinder);

		mfm_blockedWrite(head);
		crc_shiftByte(head);

		//unsigned char sectorId=floppy_iso_getSectorNum(sector);
		if (geometry_iso_cpcSectorIdMode)
		{
			mfm_blockedWrite(sector | 0xC0);
			crc_shiftByte(sector | 0xC0);
		}
		else
		{
			mfm_blockedWrite(sector);
			crc_shiftByte(sector);
		}

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

		mfm_configureWrite(MFM_RAW,16);
		for (i=0;i<3;i++)
		{
			mfm_blockedWrite(0x4489);
			crc_shiftByte(0xa1);
		}
		mfm_configureWrite(MFM_ENCODE,8);

		sectorData=&((uint8_t*)trackBuffer)[((sector-1)+(head * geometry_sectors)) * geometry_payloadBytesPerSector];
		//printf("sectorData calc %d %d %d %d %p %p\n",sector, head, geometry_sectors, geometry_payloadBytesPerSector,sectorData,&trackBuffer);
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
	return 0;
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
			printf("SecHead: %d %d %x\n",header_cyl,header_head,header_sec);

			if (geometry_iso_cpcSectorIdMode)
				header_sec&=0xf; //remove 0xc0

			if (!trackSectorDetected[(header_sec-1)+(expectedHead * MAX_SECTORS_PER_TRACK)])
			{
				sectorsDetected++;
				trackSectorDetected[(header_sec-1)+(expectedHead * MAX_SECTORS_PER_TRACK)]=1;
			}

			if (header_cyl!=expectedCyl)
			{
				header_cyl=0;
				header_head=0;
				header_sec=0;
				//printf("Cylinder is wrong!\n");
				mfm_errorHappened=1;
			}

			if (header_head != expectedHead)
			{
				header_cyl=0;
				header_head=0;
				header_sec=0;
				//printf("Head is wrong!\n");
				mfm_errorHappened=1;
			}

			if (header_sec > geometry_sectors)
			{
				printf("Ignore Sector!\n");

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

		if (verifyMode)
		{
			if (sectorData[i]!=mfm_decodedByte)
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

			//set this sector as a verified / read out one
			if (!trackSectorRead[(header_sec-1)+(expectedHead * MAX_SECTORS_PER_TRACK)])
			{
				sectorsRead++;
				trackSectorRead[(header_sec-1)+(expectedHead * MAX_SECTORS_PER_TRACK)]=1;
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




int floppy_iso_calibrateTrackLength()
{
	unsigned int resultGood=0;
	int trys=30;

	int i;

	printf("floppy_iso_calibrateTrackLength\n");
	while (!resultGood)
	{
		if (floppy_iso_writeTrack(0,0,1))
			return 1;

		//now lets be sure because of write gate latency and add some bytes
		for (i=0;i<10;i++)
			mfm_blockedWrite(0x4E);

		if (indexHappened)
		{
			printf("Iso Track was too long: %d %d   %d %d   %d %d\n",
					(int)geometry_iso_trackstart_4e,
					(int)geometry_iso_trackstart_00,
					(int)geometry_iso_before_idam_4e,
					(int)geometry_iso_before_idam_00,
					(int)geometry_iso_before_data_4e,
					(int)geometry_iso_before_data_00);

			if (geometry_iso_trackstart_4e > 3)
				geometry_iso_trackstart_4e-=3;

			if (geometry_iso_trackstart_4e > 3)
				geometry_iso_trackstart_4e-=3;



			if (geometry_iso_trackstart_00 > 0)
				geometry_iso_trackstart_00-=1;


			if (geometry_iso_before_idam_4e > 2)
				geometry_iso_before_idam_4e-=2;

			if (geometry_iso_before_idam_4e > 2)
				geometry_iso_before_idam_4e-=2;



			if (geometry_iso_before_idam_00 > 2)
				geometry_iso_before_idam_00-=1;

			trys--;
			if (!trys)
			{
				printf("Give up...\n");
				resultGood=1;
			}
			else
			{
				printf("%d\n",trys);
			}

		}
		else
		{
			printf("Iso Track parameters are fine: %d %d   %d %d   %d %d\n",
					(int)geometry_iso_trackstart_4e,
					(int)geometry_iso_trackstart_00,
					(int)geometry_iso_before_idam_4e,
					(int)geometry_iso_before_idam_00,
					(int)geometry_iso_before_data_4e,
					(int)geometry_iso_before_data_00);

			resultGood=1;
		}
	}

	return 0;
}

