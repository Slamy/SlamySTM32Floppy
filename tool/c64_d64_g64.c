/*
 * c64.c
 *
 *  Created on: 06.04.2016
 *      Author: andre
 */

#include <math.h>
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


int floppy_c64_getSectorAnz(int trk)
{
	if (trk <= 16) //Track 1 - 17 -> Track 0 - 16
	{
		return 21;
	}
	else if (trk <= 23) //Track 18 - 24 -> Track 17 - 23
	{
		return 19;
	}
	else if (trk <= 29) //Track 25 - 30 -> Track 24 - 29
	{
		return 18;
	}
	else //Track 31 - Ende
	{
		return 17;
	}
}


int readImage_d64(const char *path)
{
	int geometry_payloadBytesPerSector=256;
	int geometry_sectors;
	int geometry_tracks;
	struct stat info;

	assert(!stat(path, &info));
	int diskSize=info.st_size;

	switch (diskSize)
	{
		case 174848: //Standard D64
			printf("Standard D64\n");
			geometry_tracks=35;
			geometry_cylinders=geometry_tracks*2;
			geometry_heads=1;
			break;

		default:
			printf("D64 has no usable size!\n");
			exit(1);
	}
	FILE *f=fopen(path,"r");
	assert(f);

	int trk,sec;
	int cyl=0;
	int totalBytesRead=0;

	for (trk=0;trk<geometry_tracks;trk++)
	{
		//Für jede C64 Track legen wir 2 Cylinder an. Zuerst die eigentlichen Daten in der geraden Spur
		geometry_sectors=floppy_c64_getSectorAnz(trk);
		geometry_sectorsPerCylinder[cyl]=geometry_sectors;
		image_cylinderSize[cyl]=geometry_sectors * geometry_payloadBytesPerSector;

		int bytesRead = fread(&image_cylinderBuf[cyl][0],1,image_cylinderSize[cyl],f);
		assert(image_cylinderSize[cyl] == bytesRead);

		cyl++;

		//Dann die ungerade Spur mit negativen Größen.
		//geometry_sectorsPerCylinder[cyl]=-1;
		//image_cylinderSize[cyl]=-1;

		geometry_sectorsPerCylinder[cyl]=0;
		image_cylinderSize[cyl]=0;


		cyl++;
	}

	//um sicherzugehen, dass es auch wirklich nichts mehr zu lesen gibt
	char dummyBuf[512];
	int bytesRead = fread(dummyBuf,1,256,f);
	assert(bytesRead==0);

	fclose(f);

	return 0;
}



#if 0
#define BM_MATCH       	0x10 /* not used but exists in very old images */

int readImage_nib(const char *path)
{
	struct stat info;

	assert(!stat(path, &info));
	int diskSize=info.st_size;

	FILE *f=fopen(path,"r");
	assert(f);

	unsigned char *file_buffer=malloc(diskSize);
	assert(file_buffer);
	int bytesRead = fread(file_buffer,1,diskSize,f);
	assert(bytesRead==diskSize);

	fclose(f);

	int t_index=0, h_index=0;

	printf("\nParsing NIB data...\n");

	if (memcmp(file_buffer, "MNIB-1541-RAW", 13) != 0)
	{
		printf("Not valid NIB data!\n");
		return 0;
	}
	else
		printf("NIB file version %d\n", file_buffer[13]);

	while(file_buffer[0x10+h_index])
	{
		int track = file_buffer[0x10+h_index];
		int track_density=(unsigned char)(file_buffer[0x10 + h_index + 1]);
		track_density %= BM_MATCH;  	 /* discard unused BM_MATCH mark */

		printf("track %d %d\n",track,track_density);
#if 0
		track_density[track] = (BYTE)(file_buffer[0x10 + h_index + 1]);
		track_density[track] %= BM_MATCH;  	 /* discard unused BM_MATCH mark */

		memcpy(track_buffer + (track * NIB_TRACK_LENGTH),
			file_buffer + (t_index * NIB_TRACK_LENGTH) + 0x100,
			NIB_TRACK_LENGTH);
#endif
		h_index+=2;
		t_index++;
	}
	printf("Successfully parsed NIB data for %d tracks\n", t_index);


	return 0;

}

#endif

unsigned int speed1541[]={
	227,
	246,
	262,
	280
};

int readImage_g64(const char *path)
{

	struct stat info;

	assert(!stat(path, &info));
	int diskSize=info.st_size;

	FILE *f=fopen(path,"r");
	assert(f);

	unsigned char *diskImage=malloc(diskSize);
	assert(diskImage);
	int bytesRead = fread(diskImage,1,diskSize,f);
	assert(bytesRead==diskSize);

	fclose(f);

	char *header=diskImage;
	uint32_t trackOffsets[90];
	uint32_t speedOffsets[90];


	if (strncmp(header,"GCR-1541",8))
	{
		printf("GCR Header nicht gefunden!\n");
		return 1;
	}

	unsigned int globalTrackSize=((unsigned int)header[0xa]&0xff) | (((unsigned int)header[0xb]&0xff) <<8 );
	int tracksAnz=header[0x9];
	printf("GCR Version: %x  Tracks Anz: %d Track Size: %d\n",
			header[0x8],
			tracksAnz,
			globalTrackSize
			);

	memcpy(trackOffsets, &diskImage[0xc], sizeof(uint32_t)*tracksAnz);
	memcpy(speedOffsets, &diskImage[0xc+sizeof(uint32_t)*tracksAnz], sizeof(uint32_t)*tracksAnz);

	int cyl;
	geometry_cylinders=0;
	geometry_heads=1;

	for (cyl=0;cyl<tracksAnz;cyl++)
	{

		if (trackOffsets[cyl])
		{
			unsigned char *trackData=&diskImage[trackOffsets[cyl]];
			unsigned int actualTrackSize=((unsigned int)trackData[0]&0xff) | (((unsigned int)trackData[1]&0xff) <<8 );

			int imageCellLength=speed1541[3-speedOffsets[cyl]];
			int possibleCelllength=floor( ((double)CELL_TICKS_PER_ROTATION_360) / ((double)actualTrackSize * 8.0));

			int resultingCellLength;

			if (possibleCelllength < imageCellLength)
				resultingCellLength = possibleCelllength;
			else
				resultingCellLength = imageCellLength;

			//resultingCellLength = possibleCelllength;
			//resultingCellLength = imageCellLength;


#if 0
			//Hack für Katakis Side 1 G64
			if (cyl==70)
			{
				printf("Katakis Cyl 70 Hack !! **********************************\n");
				resultingCellLength = imageCellLength;
			}
#endif

			printf("%d %.1f %x %x TrackSize: %d   %x %x    %d %d  ->  %d\n",
					cyl,
					1.0f + (float)cyl/2.0f,
					trackOffsets[cyl],
					speedOffsets[cyl],
					actualTrackSize,
					trackData[2],
					trackData[2+actualTrackSize-1],
					imageCellLength,
					possibleCelllength,
					resultingCellLength
					);

			assert(speedOffsets[cyl] < 4); //Aktuell nur ohne Speed offset table

			int cylBufIndex=0;


			//Header für Raw Daten mit Density Infos
			image_cylinderBuf[cyl][cylBufIndex]=actualTrackSize>>8;
			image_cylinderBuf[cyl][cylBufIndex+1]=actualTrackSize&0xff;
			image_cylinderBuf[cyl][cylBufIndex+2]=2;
			cylBufIndex+=3;



			//if (cyl==70)
			//if (cyl==68)
			//if (cyl==34)
			/*
			if (cyl==48)
			{
				printf("Celllen is %d\n",resultingCellLength);
				for (int i=0; i< actualTrackSize; i++)
				{

					if ((i%16)==0)
						printf("\n %06d ",i);

					printf("%02x ",trackData[2+i]);

				}
				printf("\n");
			}
			*/


			/*
			for (int i=0; i< actualTrackSize; i++)
			{
				if (!trackData[2+i])
				{
				printf("Null bei %d\n",i);
				}
			}
			*/

			memcpy (&image_cylinderBuf[cyl][cylBufIndex], &trackData[2], actualTrackSize);
			cylBufIndex+=actualTrackSize;

			//Header für Density Data
			image_cylinderBuf[cyl][cylBufIndex+0]=0;
			image_cylinderBuf[cyl][cylBufIndex+1]=8;
			image_cylinderBuf[cyl][cylBufIndex+2]=4; //markiert variable density data
			cylBufIndex+=3;

			//Variable Density Data

			image_cylinderBuf[cyl][cylBufIndex+0]=0;
			image_cylinderBuf[cyl][cylBufIndex+1]=0;
			image_cylinderBuf[cyl][cylBufIndex+2]=resultingCellLength>>8;
			image_cylinderBuf[cyl][cylBufIndex+3]=resultingCellLength&0xff;
			cylBufIndex+=4;

			image_cylinderBuf[cyl][cylBufIndex+0]=0xff;
			image_cylinderBuf[cyl][cylBufIndex+1]=0xff;
			image_cylinderBuf[cyl][cylBufIndex+2]=0;
			image_cylinderBuf[cyl][cylBufIndex+3]=0;
			cylBufIndex+=4;


			//Kennzeichne das Ende
			image_cylinderBuf[cyl][cylBufIndex]=0;
			image_cylinderBuf[cyl][cylBufIndex+1]=0;
			image_cylinderBuf[cyl][cylBufIndex+2]=0;
			cylBufIndex+=3;

			image_cylinderSize[cyl]=cylBufIndex;
			geometry_sectorsPerCylinder[cyl]=1; //bei raw nehmen wir an, es gibt einen großen sector

			if (cyl >= geometry_cylinders)
				geometry_cylinders=cyl + 1;
		}

	}

	return 0;
}

