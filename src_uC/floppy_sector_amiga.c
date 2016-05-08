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
#include "assert.h"

uint32_t byteSwap(uint32_t num)
{
	return ((num>>24)&0xff) | // move byte 3 to byte 0
	                    ((num<<8)&0xff0000) | // move byte 1 to byte 2
	                    ((num>>8)&0xff00) | // move byte 2 to byte 1
	                    ((num<<24)&0xff000000); // byte 0 to byte 3
}

int floppy_amiga_writeTrack(uint32_t cylinder, uint32_t head)
{
	int sector,i;
	uint32_t amiga_sectorHeader; //4 byte header
	uint32_t amiga_checksum[20];
	uint32_t *trackBuf;
	uint32_t wordToSend;

	//Die Datenchecksumme aller Sektoren MUSS vorher feststehen.

	assert ((head * geometry_sectors * geometry_payloadBytesPerSector) < CYLINDER_BUFFER_SIZE);
	trackBuf=&cylinderBuffer[head*geometry_sectors*geometry_payloadBytesPerSector/4];

	for (sector=0; sector < geometry_sectors; sector++)
	{
		amiga_checksum[sector]=0;

		for (i=0; i < 128; i++)
		{
			wordToSend=byteSwap(*trackBuf);
			amiga_checksum[sector]^=wordToSend & AMIGA_MFM_MASK;
			amiga_checksum[sector]^=(wordToSend>>1) & AMIGA_MFM_MASK;
			trackBuf++;
		}
	}

	assert ((head * geometry_sectors * geometry_payloadBytesPerSector) < CYLINDER_BUFFER_SIZE);
	trackBuf=&cylinderBuffer[head*geometry_sectors*geometry_payloadBytesPerSector/4];

	if (floppy_waitForIndex())
		return 1;

	floppy_setWriteGate(1);

	flux_configureWrite(FLUX_MFM_ENCODE,8);
	flux_configureWriteCellLength(0);
	flux_blockedWrite(0x00);

	//Ist das wirklich notwendig? Wir warten auf den Index und löschen die ganze Spur zur Sicherheit einmal...
		
	if (floppy_waitForIndex())
		return 1;

	//Am Anfang des Tracks ein paar 0 Bytes? Wie viele eigentlich? Und warum ?

	//printf("TrackDat: %p %x\n",&trackBuf[0],trackBuf[0]);
	//printf("TrackDat: %p %x\n",&trackBuf[254%128],trackBuf[254%128]);

	flux_configureWrite(FLUX_MFM_ENCODE,8);

	for (i=0;i<10;i++)
		flux_blockedWrite(0x00);


	for (sector=0; sector < geometry_sectors; sector++)
	{
		//Jeder Sektor beginnt mit 2x Bytes und 2 A1 sync words
		flux_configureWrite(FLUX_MFM_ENCODE,8);
		for (i=0;i<2;i++)
			flux_blockedWrite(0x00);

		flux_configureWrite(FLUX_RAW,16);
		for (i=0;i<2;i++)
		{
			flux_blockedWrite(0x4489);
		}

		//Jetzt kommt das Sector Header Longword
		/* Beispiele für Cylinder 0 und Head 0. Sektoren 0 bis 10.
		 * AmiSec: ff00000b
		 * AmiSec: ff00010a
		 * ....
		 * AmiSec: ff000803
		 * AmiSec: ff000902
		 * AmiSec: ff000a01
		 */
		amiga_sectorHeader=0xff000000 | (cylinder<<17) | (head<<16) | (sector<<8) | (geometry_sectors - sector);

		flux_configureWrite(FLUX_MFM_ENCODE_ODD,16);
		flux_blockedWrite(amiga_sectorHeader);
		flux_blockedWrite(amiga_sectorHeader<<1);

		//OS recovery info ist einfach 0. 16 byte also 4 longwords

		flux_blockedWrite(0);
		flux_blockedWrite(0);
		flux_blockedWrite(0);
		flux_blockedWrite(0);
		//mfm_blockedWrite(0x42424242);

		flux_blockedWrite(0);
		flux_blockedWrite(0);
		flux_blockedWrite(0);
		flux_blockedWrite(0);
		//mfm_blockedWrite(0x42424242<<1);

		//jetzt kommt die header checksumme. das ist speziell in diesem fall einfach nochmal der sektor header
		flux_blockedWrite(0);
		/*
		printf("%x %x\n",
				(amiga_sectorHeader>>1) & AMIGA_MFM_MASK,
				amiga_sectorHeader& AMIGA_MFM_MASK);
		*/

		flux_blockedWrite(((((amiga_sectorHeader>>1) & AMIGA_MFM_MASK) ^ (amiga_sectorHeader& AMIGA_MFM_MASK)))<<1);

		//jetzt die datenchecksumme die vorher berechnet wurde
		flux_blockedWrite(0);
		flux_blockedWrite(amiga_checksum[sector]<<1);
		//mfm_blockedWrite(0x42424242);
		//mfm_blockedWrite(0x42424242<<1);

		/*
		//nun folgen 512 byte daten. das sind 128 langwörter
		for (i=0; i<128; i++)
		{
			wordToSend=byteSwap(*trackBuf);

			mfm_blockedWrite(wordToSend);
			mfm_blockedWrite(wordToSend<<1);
			trackBuf++;
		}
		*/

		//printf("first %lx\n",trackBuf[0]);
		//mfm_blockedWrite(0x42424242);
		//mfm_blockedWrite(amiga_sectorHeader);
		//mfm_blockedWrite(amiga_sectorHeader<<1);

		for (i=0; i<128; i++)
		{
			wordToSend=byteSwap(trackBuf[i]);
			flux_blockedWrite(wordToSend);
			//mfm_blockedWrite(0xffffffff);
		}

		for (i=0; i<128; i++)
		{
			wordToSend=byteSwap(trackBuf[i]);
			flux_blockedWrite(wordToSend<<1);
		}

		trackBuf+=128;

	}

	//Spur auslaufen lassen...
	for (i=0;i<2;i++)
		flux_blockedWrite(0);

	floppy_setWriteGate(0);


	return 0;
}

int floppy_amiga_readTrackMachine(int expectedCyl, int expectedHead)
{
	static uint32_t *sectorData;

	static uint32_t amiga_sectorHeader; //4 byte header
	static uint32_t amiga_rawMfm_unshifted[8]; //4 byte even und odd für debugging
	static uint32_t amiga_checksum;

	static uint32_t header_sec=0;
	static uint32_t header_cyl=0;
	static uint32_t header_head=0;

	//static uint32_t debug1,debug2;

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
			amiga_rawMfm_unshifted[0]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1;
			amiga_checksum^=(mfm_savedRawWord & AMIGA_MFM_MASK);

			trackReadState++;
			break;
		case 2:
			//Fortsetzung des 4 Byte Blocks
			mfm_blockedRead();

			amiga_sectorHeader|=(mfm_savedRawWord & AMIGA_MFM_MASK); //Even Byte
			amiga_rawMfm_unshifted[1]=(mfm_savedRawWord & AMIGA_MFM_MASK);
			amiga_checksum^=(mfm_savedRawWord & AMIGA_MFM_MASK);

			trackReadState++;
			//printf("AmiSec BeforeChecksum: %x\n",amiga_sectorHeader);
			i=0;
			break;

		case 3:
			//16 Byte Block

			mfm_blockedRead();
			//printf("16 byte block %d\n",i);

			/*
			if (i==3)
				debug1=mfm_savedRawWord & AMIGA_MFM_MASK;
			if (i==7)
				debug2=mfm_savedRawWord & AMIGA_MFM_MASK;
			*/

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

			if (i >=1)
				amiga_rawMfm_unshifted[3]=mfm_savedRawWord & AMIGA_MFM_MASK;
			else
				amiga_rawMfm_unshifted[2]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1;

			amiga_checksum^=mfm_savedRawWord & AMIGA_MFM_MASK;
			//printf("AmiSec BeforeChecksum: %x\n",amiga_sectorHeader);
			i++;
			if (i >=2)
			{

				if (!amiga_checksum)
				{
					//Die Header-Checksumme ist 0. Das ist gut!

					//printf("AmiSec: %x\n",amiga_sectorHeader);
					/*
					printf("%x %x %x %x\n",
							amiga_rawMfm_unshifted[0],
							amiga_rawMfm_unshifted[1],
							amiga_rawMfm_unshifted[2],
							amiga_rawMfm_unshifted[3]);
					*/

					header_sec=(amiga_sectorHeader>>8)&0xff;
					header_head=(amiga_sectorHeader>>16)&0x1;
					header_cyl=(amiga_sectorHeader>>17)&0x7f;

					if (header_cyl!=expectedCyl)
					{
						//printf("Cylinder is wrong %d != %d in %x\n",header_cyl,expectedCyl,amiga_sectorHeader);
						header_cyl=0;
						header_head=0;
						header_sec=0;
						mfm_errorHappened=1;
						//return 1;
					}

					if (header_head != expectedHead)
					{
						printf("Head is wrong: %d %d\n",(int)header_head, (int)expectedHead);
						header_cyl=0;
						header_head=0;
						header_sec=0;
						mfm_errorHappened=1;
						//return 2;
					}

					if (!trackSectorDetected[header_sec+(header_head * MAX_SECTORS_PER_TRACK)])
					{
						sectorsDetected++;
						trackSectorDetected[header_sec+(header_head * MAX_SECTORS_PER_TRACK)]=1;
					}

					trackReadState++;
					amiga_checksum=0;
					i=0;
				}
				else
				{
					//printf("%x %x\n",debug1,debug2);
					printf("head chksum err\n");
					printf("%x %x %x %x\n",
							(unsigned int)amiga_rawMfm_unshifted[0],
							(unsigned int)amiga_rawMfm_unshifted[1],
							(unsigned int)amiga_rawMfm_unshifted[2],
							(unsigned int)amiga_rawMfm_unshifted[3]);
					trackReadState=0;
				}
			}
			break;
		case 5:
			//Daten Checksumme ist ein 4 Byte Block

			mfm_blockedRead();

			if (i >=1)
				amiga_rawMfm_unshifted[5]=mfm_savedRawWord & AMIGA_MFM_MASK;
			else
				amiga_rawMfm_unshifted[4]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1;

			amiga_checksum^=mfm_savedRawWord & AMIGA_MFM_MASK;

			i++;
			if (i >=2)
			{
				//Die Daten Checksumme wurde gesichert und wird gleich benötigt.

				sectorData=&cylinderBuffer[(header_head * geometry_sectors + header_sec) * geometry_payloadBytesPerSector/4];
				//printf("header_sec:%d %d %d\n",header_sec,sectorData-trackBuffer,sizeof(trackBuffer));
				trackReadState++;
				i=0;

			}
			break;
		case 6:
			//512 Byte Datenblock. Das sind 128 Longwords. 128 Odd zuerst. 128 Even danach.
			mfm_blockedRead();

			if (mfm_errorHappened)
			{
				printf("FEHLER!\n");
			}
#if 1
			if (i>=128)
			{
				//Even Part of Longword
				if (verifyMode)
				{
					if ((byteSwap(sectorData[i%128]) & AMIGA_MFM_MASK)!=((mfm_savedRawWord & AMIGA_MFM_MASK)))
					{
						printf("even %x %p %d %x %x\n",
								(unsigned int)amiga_sectorHeader,
								&sectorData[i%128],
								(int)i,
								(unsigned int)sectorData[i%128],
								(unsigned int)mfm_savedRawWord & AMIGA_MFM_MASK);
						return 3; //verify failed
					}
				}
				else
				{
					sectorData[i%128]|=(mfm_savedRawWord & AMIGA_MFM_MASK);
					sectorData[i%128]=byteSwap(sectorData[i%128]);
				}

				//if (header_sec==0 && i==128 && header_head==0)
				//	printf("e%lx\n",(mfm_savedRawWord & AMIGA_MFM_MASK));
			}
			else
			{
				//Odd Part of Longword
				if (verifyMode)
				{
					if (((byteSwap(sectorData[i%128]>>1) & AMIGA_MFM_MASK))!=(mfm_savedRawWord & AMIGA_MFM_MASK))
					{
						printf("odd %x %p %d %x %x\n",
								(unsigned int)amiga_sectorHeader,
								&sectorData[i%128],
								(int)i,
								(unsigned int)sectorData[i%128],
								(unsigned int)mfm_savedRawWord & AMIGA_MFM_MASK);
						return 3; //verify failed
					}
				}
				else
				{
					sectorData[i%128]=(mfm_savedRawWord & AMIGA_MFM_MASK)<<1;
				}

				//if (header_sec==0 && i==0 && header_head==0)
				//	printf("o%lx\n",(mfm_savedRawWord & AMIGA_MFM_MASK)<<1);
			}
#endif

			amiga_checksum^=mfm_savedRawWord & AMIGA_MFM_MASK;

			i++;
			if (i >= 256)
			{
				if (!amiga_checksum)
				{
					//Die Daten-Checksumme ist 0. Das ist gut!

					//printf("AmiSec Dat: %d %d %d\n",header_cyl,header_head,header_sec);

					if (amiga_rawMfm_unshifted[4] || amiga_rawMfm_unshifted[2])
					{
						printf("First word nicht 0!\n");
					}

					if (!trackSectorRead[header_sec+(header_head * MAX_SECTORS_PER_TRACK)])
					{
						sectorsRead++;
						trackSectorRead[header_sec+(header_head * MAX_SECTORS_PER_TRACK)]=1;
					}

					lastSectorDataFormat=0xff;

					//Setze Status zurück...
					header_cyl=0;
					header_head=0;
					header_sec=0;

				}
				else
				{
					printf("data chksum err %x\n",(unsigned int)amiga_checksum);
					printf("%x %x %x %x %x %x\n",
							(unsigned int)amiga_rawMfm_unshifted[0],
							(unsigned int)amiga_rawMfm_unshifted[1],
							(unsigned int)amiga_rawMfm_unshifted[2],
							(unsigned int)amiga_rawMfm_unshifted[3],
							(unsigned int)amiga_rawMfm_unshifted[4],
							(unsigned int)amiga_rawMfm_unshifted[5]);

				}

				trackReadState=0;
			}
			break;
		default:
			trackReadState=0;
	}

	return 0;
}
