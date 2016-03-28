
#include <libusb-1.0/libusb.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <caps/capsimage.h>

#include "slamySTM32Floppy.h"


int readImage_amstradCpc(const char *path)
{
	unsigned char diskInfoBlock[256];
	unsigned char trackInfoBlock[256];
	unsigned char unusedBuffer[8192];

	FILE *f=fopen(path,"r");
	assert(f);

	//Read disk information block
	int bytes_read=fread(diskInfoBlock,1,256,f);
	assert(bytes_read==256);

	if (!memcmp("MV - CPCEMU Disk-File\r\nDisk-Info\r\n",diskInfoBlock,34))
	{
		assert(0);
	}
	else if(!memcmp("EXTENDED CPC DSK File\r\nDisk-Info\r\n",diskInfoBlock,34))
	{
		//Name of creator
		printf("Creator:%s\n",&diskInfoBlock[0x22]);

		//Infos and reserved bytes
		int tracks=diskInfoBlock[0x30];
		int sides=diskInfoBlock[0x31];
		//int tracksize=((int)diskInfoBlock[0x32]) | (((int)diskInfoBlock[0x33])<<8);
		printf("Number of Tracks:%d\n",tracks);
		printf("Number of Sides:%d\n",sides);
		//printf("Size of a Track:%d\n",tracksize);

		geometry_cylinders=tracks;
		geometry_heads=sides;
		//Sectoranzahl wird für jeden Cylinder festgelegt.

		int i,j;
		for (i=0;i<tracks*sides;i++)
		{
			int trackSize=diskInfoBlock[0x34+i];
			printf("Track %d -> Cyl:%d Head:%d Size %d\n",i,i>>1,i&1,trackSize);
		}


		int dataPos=0;
		for (i=0;i<tracks*sides;i++)
		{
			int trackSize=diskInfoBlock[0x34+i];

			if (trackSize)
			{
				//Für jeden Track den Track Information Block lesen
				bytes_read=fread(trackInfoBlock,1,256,f);
				assert(bytes_read == 256);
				//printf("X%sX\n",temp);
				assert(!memcmp("Track-Info\r\n" ,trackInfoBlock,12));

				unsigned int track=trackInfoBlock[0x10];
				unsigned int side=trackInfoBlock[0x11];
				unsigned int sectorSize=trackInfoBlock[0x14];
				unsigned int sectors=trackInfoBlock[0x15];
				unsigned int gap3len=trackInfoBlock[0x16];
				unsigned int fillerByte=trackInfoBlock[0x17];

				if (side==0)
					dataPos=0;

				printf("Track %d %d %d %d\n",track,side,sectorSize,sectors);

				if (side==0)
				{
					geometry_sectorsPerCylinder[track]=sectors;
					geometry_iso_gap3length[track]=gap3len;
					geometry_iso_fillerByte[track]=fillerByte;
				}
				else
				{
					if (geometry_sectorsPerCylinder[track]!=sectors)
					{
						printf("Bei dieser Amstrad CPC Disk unterscheiden sich die Tracks eines Cylinders anhand ihrer Anzahl.\nDas geht gerade nicht!\n");
						exit(1);
					}
				}

				//Jeder Track Information Block hat seine Sector Information List
				for (j=0;j<sectors;j++)
				{
					unsigned int sectorheader_cylinder=trackInfoBlock[0x18+j*8+0];
					unsigned int sectorheader_side=trackInfoBlock[0x18+j*8+1];
					unsigned int sectorheader_sector=trackInfoBlock[0x18+j*8+2];
					unsigned int sectorheader_sectorsize=trackInfoBlock[0x18+j*8+3];
					unsigned int sectorHeader_fdcStat1=trackInfoBlock[0x18+j*8+4];
					unsigned int sectorHeader_fdcStat2=trackInfoBlock[0x18+j*8+5];
					unsigned int actualLength=((int)trackInfoBlock[0x18+j*8+6]) | (((int)trackInfoBlock[0x18+j*8+7])<<8);

					printf("Sector Cyl:%d  Side:%d SectorId:%x SectorSize:%d Stat1:%x Stat2:%x ActLen:%d\n",
							sectorheader_cylinder,
							sectorheader_side,
							sectorheader_sector,
							sectorheader_sectorsize,
							sectorHeader_fdcStat1,
							sectorHeader_fdcStat2,
							actualLength);


					//assert(sectorheader_sectorsize==2);
					assert(track == sectorheader_cylinder);

					if (side==0)
					{
						geometry_iso_sectorId[track][j]=sectorheader_sector;
						geometry_iso_sectorHeaderSize[track][j]=sectorheader_sectorsize;
						geometry_iso_sectorErased[track][j]=(sectorHeader_fdcStat2 & 0x40);
						geometry_actualSectorSize[track][j]=actualLength;

						//printf("geometry_iso_sectorPos [%d][%d] = %d\n",track,j,(sectorheader_sector&0xf));
					}
					else
					{
						if (geometry_iso_sectorId[track][j]!=sectorheader_sector)
						{
							printf("Bei dieser Amstrad CPC Disk unterscheiden sich die Tracks eines Cylinders anhand der Sektor IDs.\nDas geht gerade nicht!\n");
							exit(1);
						}

						if (geometry_iso_sectorHeaderSize[track][j]!=sectorheader_sectorsize)
						{
							printf("Bei dieser Amstrad CPC Disk unterscheiden sich die Tracks eines Cylinders anhand der Sektor Größe.\nDas geht gerade nicht!\n");
							exit(1);
						}

						if (geometry_actualSectorSize[track][j]!=actualLength)
						{
							printf("Bei dieser Amstrad CPC Disk unterscheiden sich die Tracks eines Cylinders anhand der Sektor Größe.\nDas geht gerade nicht!\n");
							exit(1);
						}
					}
				}

				//Nun die eigentlichen Daten. Die Sektoren liegen in der gleichen Reihenfolge, wie die Sektoren in der Sector Information List
				/* Beispiel anhand oberer Ausgabe:
				** geometry_iso_sectorPos [0][0] = 1
				** geometry_iso_sectorPos [0][1] = 6
				** geometry_iso_sectorPos [0][2] = 2
				** geometry_iso_sectorPos [0][3] = 7
				** geometry_iso_sectorPos [0][4] = 3
				** geometry_iso_sectorPos [0][5] = 8
				** geometry_iso_sectorPos [0][6] = 4
				** geometry_iso_sectorPos [0][7] = 9
				** geometry_iso_sectorPos [0][8] = 5
				*/

				for (j=0;j<sectors;j++)
				{
					/*
					printf("Read %d bytes...\n",isoSectorSizes[geometry_iso_sectorHeaderSize[track][j]]);

					bytes_read=fread(&image_cylinderBuf[track][dataPos],1,isoSectorSizes[geometry_iso_sectorHeaderSize[track][j]],f);
					assert(bytes_read==isoSectorSizes[geometry_iso_sectorHeaderSize[track][j]]);
					*/

					printf("Read %d bytes...\n",geometry_actualSectorSize[track][j]);
					bytes_read=fread(&image_cylinderBuf[track][dataPos],1,geometry_actualSectorSize[track][j],f);
					assert(bytes_read==geometry_actualSectorSize[track][j]);

					printf("Sector Cyl:%d SectorId:%x Data %02x ... %02x\n",
							track,
							j,
							image_cylinderBuf[track][dataPos],
							image_cylinderBuf[track][dataPos+bytes_read-1]);

					//printf("1. Byte %x\n",image_cylinderBuf[track][geometry_payloadBytesPerSector*(sectorPos+sectors*side)]);

					dataPos+=geometry_actualSectorSize[track][j];
					image_cylinderSize[track]+=geometry_actualSectorSize[track][j];

					int moduloPos=geometry_actualSectorSize[track][j]%256;

					if (moduloPos)
					{

						int stillToRead=256-moduloPos;
						printf("Hab %d Bytes gelesen. Ich lese also noch %d\n",bytes_read,stillToRead);
						bytes_read=fread(unusedBuffer,1,stillToRead,f);
						assert(bytes_read==stillToRead);
					}
					/*
					if (isoSectorSizes[geometry_iso_sectorHeaderSize[track][j]] != geometry_actualSectorSize[track][j])
					{
						printf("Der Sektor hat %d Nutzbytes. Es müssen aber %d Byte aus dem Image gelesen werden!\n",
								geometry_actualSectorSize[track][j],isoSectorSizes[geometry_iso_sectorHeaderSize[track][j]]);

						int stillToRead=isoSectorSizes[geometry_iso_sectorHeaderSize[track][j]]-geometry_actualSectorSize[track][j];
						bytes_read=fread(unusedBuffer,1,stillToRead,f);
						assert(bytes_read==stillToRead);

					}
					*/
				}

				printf("image_cylinderSize[track %d] %d\n",track,dataPos);
			}
		}

		fclose(f);
		format=FLOPPY_FORMAT_ISO_DD;
	}
	else
		assert(0);
	return 0;
}



