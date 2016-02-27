/*
 * floppy_sector_amiga.c
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

uint32_t byteSwap(uint32_t num)
{
	return ((num>>24)&0xff) | // move byte 3 to byte 0
	                    ((num<<8)&0xff0000) | // move byte 1 to byte 2
	                    ((num>>8)&0xff00) | // move byte 2 to byte 1
	                    ((num<<24)&0xff000000); // byte 0 to byte 3
}

int floppy_amiga_readTrackMachine(int expectedCyl, int expectedHead)
{
	static uint32_t *sectorData;

	static uint32_t amiga_sectorHeader; //4 byte header
	//static uint32_t amiga_rawMfm_unshifted[2]; //4 byte even und odd für debugging
	static uint32_t amiga_checksum;

	static uint32_t header_sec=0;
	static uint32_t header_cyl=0;
	static uint32_t header_head=0;

	static uint32_t i=0;

	if (mfm_errorHappened)
	{
		//printf("R\n");
		mfm_errorHappened=0;
		trackReadState=0;
	}

	switch (trackReadState)
	{

		case 0:
			//Wir warten auf ein Sync Word bestehend aus 2 kaputten A1 Bytes
			//printf("mfm_inSync=0;\n");
			//usleep(1000*100);
			mfm_inSync=0;
			mfm_blockedWaitForSyncWord(1);
			trackReadState++;
			amiga_checksum=0;

			i=0;
			break;
		case 1:
			//printf("S\n");
			//4 Byte Block mit Header Infos
			mfm_blockedRead();

			amiga_sectorHeader=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1; //Odd Byte
			//amiga_rawMfm_unshifted[0]=(mfm_savedRawWord & AMIGA_MFM_MASK);
			amiga_checksum^=(mfm_savedRawWord & AMIGA_MFM_MASK);

			trackReadState++;
			break;
		case 2:
			//Fortsetzung des 4 Byte Blocks
			mfm_blockedRead();

			amiga_sectorHeader|=(mfm_savedRawWord & AMIGA_MFM_MASK); //Even Byte
			//amiga_rawMfm_unshifted[1]=(mfm_savedRawWord & AMIGA_MFM_MASK);
			amiga_checksum^=(mfm_savedRawWord & AMIGA_MFM_MASK);

			trackReadState++;

			i=0;
			break;

		case 3:
			//16 Byte Block

			mfm_blockedRead();
			//printf("16 byte block %d\n",i);

			amiga_checksum^=mfm_savedRawWord & AMIGA_MFM_MASK;

			i++;
			if (i >=8)
			{
				trackReadState++;
				i=0;
			}
			break;

		case 4:
			//Header Checksumme ist wieder ein 4 Byte Block
			mfm_blockedRead();

			amiga_checksum^=mfm_savedRawWord & AMIGA_MFM_MASK;

			i++;
			if (i >=2)
			{

				if (!amiga_checksum)
				{
					//Die Header-Checksumme ist 0. Das ist gut!

					//printf("AmiSec: %x\n",amiga_sectorHeader);

					header_sec=(amiga_sectorHeader>>8)&0xff;
					header_head=(amiga_sectorHeader>>16)&0x1;
					header_cyl=(amiga_sectorHeader>>17)&0x7f;

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

					trackReadState++;
					amiga_checksum=0;
					i=0;
				}
				else
				{
					printf("head chksum err\n");
					trackReadState=0;
				}
			}
			break;
		case 5:
			//Daten Checksumme ist ein 4 Byte Block

			mfm_blockedRead();

			amiga_checksum^=mfm_savedRawWord & AMIGA_MFM_MASK;

			i++;
			if (i >=2)
			{
				//Die Daten Checksumme wurde gesichert und wird gleich benötigt.

				sectorData=&trackBuffer[(header_head * geometry_sectors + header_sec) * geometry_payloadBytesPerSector/4];
				//printf("header_sec:%d %d %d\n",header_sec,sectorData-trackBuffer,sizeof(trackBuffer));
				trackReadState++;
				i=0;

			}
			break;
		case 6:
			//512 Byte Datenblock. Das sind 128 Longwords. 128 Odd zuerst. 128 Even danach.
			mfm_blockedRead();

			if (i>=128)
			{
				sectorData[i%128]|=(mfm_savedRawWord & AMIGA_MFM_MASK); //Even Byte
				sectorData[i%128]=byteSwap(sectorData[i%128]);

				//if (header_sec==0 && i==128 && header_head==0)
				//	printf("e%lx\n",(mfm_savedRawWord & AMIGA_MFM_MASK));
			}
			else
			{
				sectorData[i%128]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1; //Odd Byte
				//if (header_sec==0 && i==0 && header_head==0)
				//	printf("o%lx\n",(mfm_savedRawWord & AMIGA_MFM_MASK)<<1);
			}

			amiga_checksum^=mfm_savedRawWord & AMIGA_MFM_MASK;

			i++;
			if (i >= 256)
			{
				if (!amiga_checksum)
				{
					//Die Daten-Checksumme ist 0. Das ist gut!

					//printf("AmiSec Dat: %d %d %d\n",header_cyl,header_head,header_sec);

					if (!trackSectorRead[header_sec+(header_head * geometry_sectors)])
					{
						sectorsRead++;
						trackSectorRead[header_sec+(header_head * geometry_sectors)]=1;
					}

					lastSectorDataFormat=0xff;

				}
				else
				{
					printf("data chksum err\n");

				}

				trackReadState=0;

			}
			break;
		default:
			trackReadState=0;
	}

	return 0;
}
