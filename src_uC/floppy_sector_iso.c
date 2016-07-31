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
#include "floppy_flux_read.h"
#include "floppy_flux_write.h"
#include "assert.h"

unsigned int floppy_iso_getSectorPos(unsigned char sectorId)
{
	int i;
	for (i=0;i< geometry_sectors; i++)
	{
		if (geometry_iso_sectorId[i]==sectorId)
		{
			return i;
		}
	}

	return -1;
}

unsigned char *floppy_iso_getBufPtr(unsigned char sectorId, unsigned int head)
{
	int i;
	int bytesIntoTrack=0;

	for (i=0;i< geometry_sectors; i++)
	{
		//printf("floppy_iso_getBufPtr %x %x\n",geometry_iso_sectorId[i],sectorId);
		if (geometry_iso_sectorId[i]==sectorId)
		{
			//Wir haben den passenden Sektor gefunden. Nun die Adresse ausrechnen...

			if (head)
			{
				int lenOfOneTrack=(cylinderSize-4*geometry_sectors)/2;
				//printf("floppy_iso_getBufPtr %d %d\n",lenOfOneTrack,bytesIntoTrack);
				return &((uint8_t*)cylinderBuffer)[bytesIntoTrack+4*geometry_sectors+lenOfOneTrack];
			}
			else
			{
				//printf("bytesToInTrack:%d\n",bytesToInTrack);
				return &((uint8_t*)cylinderBuffer)[bytesIntoTrack+4*geometry_sectors];
			}

		}
		bytesIntoTrack+=geometry_actualSectorSize[i];
	}

	printf("floppy_iso_getBufPtr failed with %x %d\n",sectorId,head);
	//assert(0);
	return NULL;
}

int floppy_iso_writeTrack(int cylinder, int head)
{
	uint8_t *sectorData;
	static int iso_cellLengthDecrement=0;
	int i;
	int sectorPos;

	printf("floppy_iso_writeTrack %d %d\n",cylinder,head);

	/*
	printf("Sektors %d -> ",geometry_sectors);
	for (sector=1; sector <= geometry_sectors; sector++)
		printf("%d ",floppy_iso_getSectorNum(sector));
	printf("\n");
	*/

	int dataPos=geometry_sectors*4; //Überspringe 4 Byte pro Sektor wegen den "ISO Custom Settings"
	if (head)
	{
		for (sectorPos=0; sectorPos < geometry_sectors; sectorPos++)
			dataPos+=geometry_actualSectorSize[sectorPos];
	}

	for (sectorPos=0; sectorPos < geometry_sectors; sectorPos++)
	{
		sectorData=&((uint8_t*)cylinderBuffer)[dataPos];

		printf("%d %x %d %d %d data %02x ... %02x\n",
				sectorPos,
				geometry_iso_sectorId[sectorPos],
				geometry_iso_sectorHeaderSize[sectorPos],
				geometry_actualSectorSize[sectorPos],
				geometry_iso_sectorErased[sectorPos],
				sectorData[0],
				sectorData[geometry_actualSectorSize[sectorPos]-1]);

		dataPos+=geometry_actualSectorSize[sectorPos];
	}

	if (floppy_waitForIndex())
		return 1;

	//FIXME Welcher Zweck?
	//mfm_configureWrite(MFM_ENCODE,8);
	//mfm_configureWriteCellLength(mfm_decodeCellLength);
	//mfm_blockedWrite(0x00);

	//iso_cellLengthDecrement=6;

	//geometry_iso_gap1_postIndex=0;
	//geometry_iso_gap4_postData=0;
	//geometry_iso_gap5_preIndex=0;

	//mfm_configureWriteCellLength(MFM_BITTIME_DD/2);

	floppy_iso_setGaps();

	do
	{
		floppy_setWriteGate(1);

		//die Spur bis zum Index mit 0en vollschreiben. Vermeidet Müll, den wir nicht haben wollen
		flux_configureWrite(FLUX_MFM_ENCODE,8);
		flux_configureWriteCellLength(flux_decodeCellLength - iso_cellLengthDecrement);
		flux_blockedWrite(0x00);

		if (floppy_waitForIndex())
			return 1;

		//printf("Index!\n");

		//if (geometry_iso_trackstart_00)

#if 0
		for (i=0;i<geometry_iso_gap1_postIndex;i++)
			flux_blockedWrite(geometry_iso_fillerByte);

		{
			//Nun der Track Header, wenn überhaupt erwünscht.

			for (i=0;i<12;i++)
				flux_blockedWrite(0x00);

			flux_configureWrite(MFM_RAW,16);
			for (i=0;i<3;i++)
				flux_blockedWrite(0x5524);
			flux_configureWrite(MFM_ENCODE,8);

			flux_blockedWrite(0xFC);
		}
#endif

		for (i=0;i<geometry_iso_gap1_postIndex;i++)
			flux_blockedWrite(geometry_iso_fillerByte);


		int dataPos=geometry_sectors*4; //Überspringe 4 Byte pro Sektor wegen den "ISO Custom Settings"
		if (head)
		{
			for (sectorPos=0; sectorPos < geometry_sectors; sectorPos++)
				dataPos+=geometry_actualSectorSize[sectorPos];
		}

		//printf("%d %p %x\n",dataPos,&((uint8_t*)cylinderBuffer)[dataPos],((uint8_t*)cylinderBuffer)[dataPos]);

		for (sectorPos=0; sectorPos < geometry_sectors; sectorPos++)
		//for (sector=1; sector <= geometry_sectors; sector++)
		{
			//printf("Write Sektor %d\n",sector);

			//Jetzt die einzelnen Sektoren. Erst der Header nach einem Gap

			//Gap 2 Pre Id. Normalerweise 12x 00
			for (i=0;i<geometry_iso_gap2_preID_00;i++)
				flux_blockedWrite(0x00);

			crc=0xffff;

			flux_configureWrite(FLUX_RAW,16);
			for (i=0;i<3;i++)
			{
				flux_blockedWrite(0x4489);
				crc_shiftByte(0xa1);
			}
			flux_configureWrite(FLUX_MFM_ENCODE,8);

			flux_blockedWrite(0xfe);
			crc_shiftByte(0xfe);

			flux_blockedWrite(cylinder);
			crc_shiftByte(cylinder);

			flux_blockedWrite(head);
			crc_shiftByte(head);

			//unsigned char sectorId=floppy_iso_getSectorNum(sector);
			flux_blockedWrite(geometry_iso_sectorId[sectorPos]);
			crc_shiftByte(geometry_iso_sectorId[sectorPos]);

			flux_blockedWrite(geometry_iso_sectorHeaderSize[sectorPos]);
			crc_shiftByte(geometry_iso_sectorHeaderSize[sectorPos]);

			flux_blockedWrite(crc>>8);
			flux_blockedWrite(crc&0xff);

			//Das war der Header. Jetzt wieder ein Gap und dann die Daten.
			crc=0xffff;

			//Gap 3
			for (i=0;i<geometry_iso_gap3_postID;i++)
				flux_blockedWrite(geometry_iso_fillerByte);

			for (i=0;i<geometry_iso_gap3_preData_00;i++)
				flux_blockedWrite(0x00);

			flux_configureWrite(FLUX_RAW,16);
			for (i=0;i<3;i++)
			{
				flux_blockedWrite(0x4489);
				crc_shiftByte(0xa1);
			}
			flux_configureWrite(FLUX_MFM_ENCODE,8);

			sectorData=&((uint8_t*)cylinderBuffer)[dataPos];
			//printf("sectorData calc %d %d %d %d %p %p\n",sector, head, geometry_sectors, geometry_payloadBytesPerSector,sectorData,&trackBuffer);

			if (geometry_iso_sectorErased[sectorPos])
			{
				flux_blockedWrite(0xf8); //Deleted Data Address Mark
				crc_shiftByte(0xf8);
			}
			else
			{
				flux_blockedWrite(0xfb); //Data Address Mark
				crc_shiftByte(0xfb);
			}

			//crc=0xFFFF;
			for (i=0;i<geometry_actualSectorSize[sectorPos];i++)
			{
				flux_blockedWrite(sectorData[i]);
				crc_shiftByte(sectorData[i]);
			}

			flux_blockedWrite(crc>>8);
			flux_blockedWrite(crc&0xff);

			dataPos+=geometry_actualSectorSize[sectorPos];


			for (i=0;i<geometry_iso_gap4_postData;i++)
				flux_blockedWrite(geometry_iso_fillerByte);
		}

		if (geometry_iso_gap5_preIndex)
		{
			for (i=0;i<geometry_iso_gap5_preIndex;i++)
				flux_blockedWrite(geometry_iso_fillerByte);
		}
		else
			flux_blockedWrite(geometry_iso_fillerByte);

		flux_write_waitForUnderflow();
		floppy_setWriteGate(0);

		//zum auslaufen
		for (i=0;i<5;i++)
			flux_blockedWrite(geometry_iso_fillerByte);


		if (indexOverflowCount)
		{
			if (configuration_flags & CONFIGFLAG_ISO_NO_ROOM_REDUCE_BITRATE)
			{
				printf("floppy_iso_writeTrack index overflow %d %d\n",indexOverflowCount,iso_cellLengthDecrement);
				iso_cellLengthDecrement++;
			}
			else
			{
				printf("floppy_iso_writeTrack index overflow fail %d\n",indexOverflowCount);
				return 2; //Es war kein Platz, ich darf aber auch nichts tun, um die Situation zu verbessern!
			}

		}
	}
	while (indexOverflowCount);

	return 0;
}

int floppy_iso_readTrackMachine(int expectedCyl, int expectedHead)
{
	static uint8_t *sectorData;

	static unsigned int header_cyl=0;
	static unsigned int header_head=0;
	static unsigned int header_secPos=0;
	static unsigned int header_secId=0;
	static unsigned int header_secDel=0;
	static unsigned int header_secSize=0;

	static unsigned int i=0;

	if (floppy_readErrorHappened)
	{
		//printf("Reset statemachine\n");
		floppy_readErrorHappened=0;
		trackReadState=0;
	}

	switch (trackReadState)
	{
	case 0:
		crc=0xFFFF; //reset crc
		crcShiftedBytes=0;

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
		crc_shiftByte(mfm_decodedByte);

		switch (mfm_decodedByte)
		{
			case 0xfe:
				trackReadState=10; //ISO IDAM - Sector Header
				break;
			case 0xfb: //Data Address Mark
				header_secDel=0;
				trackReadState=20; //ISO DAM - Sector Data
				break;
			case 0xf8: //Deleted Data Address Mark
				header_secDel=1;
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
		header_secId=mfm_decodedByte;
		crc_shiftByte(header_secId);

		header_secPos=floppy_iso_getSectorPos(header_secId);

		/*
		header_secPos=header_secId;
		if (geometry_format == FLOPPY_FORMAT_ISO_DD)
			header_secPos&=0xf; //remove 0xc0
		*/

		mfm_blockedRead();
		header_secSize=mfm_decodedByte;
		crc_shiftByte(header_secSize);

		trackReadState++;
		break;
	case 11:
		mfm_blockedRead();
		crc_shiftByte(mfm_decodedByte);
		mfm_blockedRead();
		crc_shiftByte(mfm_decodedByte);

		if (crc != 0) //crc has to be 0 at the end for a correct result
		{
			printf("**** idam crc error %d %d %d\n",header_cyl,header_head,header_secId);
			header_cyl=0;
			header_head=0;
			header_secId=0;
		}
		else
		{
			printf("SecHead: %d %d %x\n",header_cyl,header_head,header_secId);

			if (!trackSectorDetected[(header_secPos)+(expectedHead * MAX_SECTORS_PER_TRACK)])
			{
				sectorsDetected++;
				trackSectorDetected[(header_secPos)+(expectedHead * MAX_SECTORS_PER_TRACK)]=1;
			}

			if (header_cyl!=expectedCyl)
			{
				header_cyl=0;
				header_head=0;
				header_secId=0;
				//printf("Cylinder is wrong!\n");
				floppy_readErrorHappened=1;
			}

			if (header_head != expectedHead)
			{
				header_cyl=0;
				header_head=0;
				header_secId=0;
				//printf("Head is wrong!\n");
				floppy_readErrorHappened=1;
			}

			if (header_secPos >= geometry_sectors)
			{
				printf("Ignore Sector!\n");

				header_cyl=0;
				header_head=0;
				header_secId=0;
			}

			if (verifyMode && header_secSize != geometry_iso_sectorHeaderSize[header_secPos])
			{
				printf("Sector Header Size doesn't match!\n");
				header_cyl=0;
				header_head=0;
				header_secId=0;
			}

			sectorData=floppy_iso_getBufPtr(header_secId,expectedHead);
		}

		trackReadState=0;

		break;

	case 20: //DAM - Sector Data

		if (header_secId==0)
			trackReadState=0; //Keine aktuellen Headerinfos. Also zurück zum Anfang!
		else
		{
			i=0;
			trackReadState++;
			//printf("sectorData calc %d %d %d %d\n",header_sec,expectedHead,geometry_sectors,geometry_payloadBytesPerSector);

			//sectorData=&((uint8_t*)cylinderBuffer)[((header_sec-1)+(expectedHead * geometry_sectors)) * geometry_payloadBytesPerSector];

			if (sectorData==NULL)
				floppy_readErrorHappened=1;

			if (verifyMode && header_secDel != geometry_iso_sectorErased[header_secPos])
			{
				printf("dam or ddam verify failed %d %d!\n",header_secDel,geometry_iso_sectorErased[header_secPos]);
				return 3;
			}
		}
		break;
	case 21:
		mfm_blockedRead();

		if (verifyMode)
		{
			if (sectorData[i]!=mfm_decodedByte)
			{
				printf("i==%d   %p %x != %x  at header_secPos %d with actual sector size %d\n",
						i,
						&sectorData[i],
						sectorData[i],
						mfm_decodedByte,
						header_secPos,
						geometry_actualSectorSize[header_secPos]);
				return 3; //verify failed
			}
		}
		else
			sectorData[i]=mfm_decodedByte;

		crc_shiftByte(sectorData[i]);
		i++;

		if (i==geometry_actualSectorSize[header_secPos])
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
			printf("**** dam crc error %x %d %d %x %d %d\n",crc,header_cyl,header_head,header_secId,geometry_actualSectorSize[header_secPos],crcShiftedBytes);
			crc_printCheckedBytes();
		}
		else
		{
			//printf("SecDat: %d %d %d %x\n",header_cyl,header_head,header_sec,sectorData[0]);

			//set this sector as a verified / read out one
			if (!trackSectorRead[(header_secPos)+(expectedHead * MAX_SECTORS_PER_TRACK)])
			{
				sectorsRead++;
				trackSectorRead[(header_secPos)+(expectedHead * MAX_SECTORS_PER_TRACK)]=1;
			}

			lastSectorDataFormat=0xfb;


		}

		//Setze Status zurück...
		header_cyl=0;
		header_head=0;
		header_secId=0;

		trackReadState=0;

		break;



	default:
		trackReadState=0;
	}

	return 0;
}

/*
unsigned char geometry_iso_gap1_postIndex;	//32x 4E

unsigned char geometry_iso_gap2_preID_00;	//12x 00

unsigned char geometry_iso_gap3_postID;		//22x 4E
unsigned char geometry_iso_gap3_preData_00;	//12x 00

unsigned char geometry_iso_gap4_postData;	//24x 4E

unsigned char geometry_iso_gap5_preIndex;	//16x 4E
*/

#if 0
unsigned char gapConfigurations[]=
{
	//Laut "Atari FD Software" die minimale Gap Anzahl
	32, //Gap 1 Post Index (4E)
	8, //Gap 2 Pre ID (00)
	22, //Gap 3a Post ID (4E)
	12, //Gap 3b Pre Data (00)
	24, //Gap 4 Post Data (4E)
	16, //Gap 5 Pre Index (4E)

	//Laut eine gute Konfig für 11 Sektoren
	10, //Gap 1 Post Index (4E)
	3, //Gap 2 Pre ID (00)
	22, //Gap 3a Post ID (4E)
	12, //Gap 3b Pre Data (00)
	2, //Gap 4 Post Data (4E)
	0, //Gap 5 Pre Index (4E)

	0xff
};

int gapConfigurationIndex=0;

int floppy_iso_reduceGap()
{
	if (gapConfigurations[gapConfigurationIndex] == 0xff)
		return 1;

	geometry_iso_gap1_postIndex=gapConfigurations[gapConfigurationIndex++];
	geometry_iso_gap2_preID_00=gapConfigurations[gapConfigurationIndex++];
	geometry_iso_gap3_postID=gapConfigurations[gapConfigurationIndex++];
	geometry_iso_gap3_preData_00=gapConfigurations[gapConfigurationIndex++];
	geometry_iso_gap4_postData=gapConfigurations[gapConfigurationIndex++];
	geometry_iso_gap5_preIndex=gapConfigurations[gapConfigurationIndex++];

	printf("floppy_iso_reduceGap %d %d %d %d %d %d\n",
			geometry_iso_gap1_postIndex,
			geometry_iso_gap2_preID_00,
			geometry_iso_gap3_postID,
			geometry_iso_gap3_preData_00,
			geometry_iso_gap4_postData,
			geometry_iso_gap5_preIndex);

	return 0;

}
#endif

void floppy_iso_setGaps()
{
	switch (geometry_sectors)
	{
	case 1: //für einen großen Sektor. Speziell zugeschnitten auf Turrican II für den CPC


#if 0
		//Läuft nicht mit Turrican 2 für den CPC
		geometry_iso_gap1_postIndex=10;
		geometry_iso_gap2_preID_00=12;
		geometry_iso_gap3_postID=22;
		geometry_iso_gap3_preData_00=12;
		geometry_iso_gap4_postData=2;
		geometry_iso_gap5_preIndex=0;
#endif

#if 0
		//funktioniert für Turrican 2 für den CPC
		geometry_iso_gap1_postIndex=60;
		geometry_iso_gap2_preID_00=12;
		geometry_iso_gap3_postID=22;
		geometry_iso_gap3_preData_00=12;
		geometry_iso_gap4_postData=40;
		geometry_iso_gap5_preIndex=40;
#endif

#if 0
		//funktioniert für Turrican 2 für den CPC
		geometry_iso_gap1_postIndex=60;
		geometry_iso_gap2_preID_00=12;
		geometry_iso_gap3_postID=22;
		geometry_iso_gap3_preData_00=12;
		geometry_iso_gap4_postData=10;
		geometry_iso_gap5_preIndex=0;
#endif

#if 0
		//funktioniert für Turrican 2 für den CPC
		geometry_iso_gap1_postIndex=40;
		geometry_iso_gap2_preID_00=12;
		geometry_iso_gap3_postID=22;
		geometry_iso_gap3_preData_00=12;
		geometry_iso_gap4_postData=2;
		geometry_iso_gap5_preIndex=0;
#endif

#if 1
		//funktioniert für Turrican 2 für den CPC
		geometry_iso_gap1_postIndex=20;
		geometry_iso_gap2_preID_00=12;
		geometry_iso_gap3_postID=22;
		geometry_iso_gap3_preData_00=12;
		geometry_iso_gap4_postData=2;
		geometry_iso_gap5_preIndex=0;
#endif

		break;

	case 9: //Standard
		geometry_iso_gap1_postIndex=60;
		geometry_iso_gap2_preID_00=12;
		geometry_iso_gap3_postID=22;
		geometry_iso_gap3_preData_00=12;
		geometry_iso_gap4_postData=40;
		geometry_iso_gap5_preIndex=40;
		break;

	case 10:
		geometry_iso_gap1_postIndex=60;
		geometry_iso_gap2_preID_00=12;
		geometry_iso_gap3_postID=22;
		geometry_iso_gap3_preData_00=12;
		geometry_iso_gap4_postData=40;
		geometry_iso_gap5_preIndex=40;
		break;

	case 11: //z.B. Turrican 1 für Atari ST
		geometry_iso_gap1_postIndex=10;
		geometry_iso_gap2_preID_00=3;
		geometry_iso_gap3_postID=22;
		geometry_iso_gap3_preData_00=12;
		geometry_iso_gap4_postData=2;
		geometry_iso_gap5_preIndex=0;
		break;
	default:
		assert(0);
	}
}

