
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


const unsigned char possibleCylinderAnz[]=
{
	38, 39, 40, 41, 42,
	78, 79, 80, 81, 82
};

const unsigned char possibleSectorAnz[]=
{
	9,10,11,18
};

int readImage_noHeader(const char *path)
{
	struct stat info;

	int i;
	int cyl,head,heads,secs;
	int geometry_sectors=0;
	int geometry_payloadBytesPerSector=512;

	assert(!stat(path, &info));
	int diskSize=info.st_size;

	for (cyl=0; cyl<sizeof(possibleCylinderAnz);cyl++)
	{
		for (heads=1; heads <=2; heads++)
		{
			for (secs=0; secs < sizeof(possibleSectorAnz); secs++)
			{
				//printf("%d %d\n",info.st_size,possibleCylinderAnz[cyl] * heads * possibleSectorAnz[secs] * geometry_payloadBytesPerSector);
				if (diskSize == possibleCylinderAnz[cyl] * heads * possibleSectorAnz[secs] * geometry_payloadBytesPerSector)
				{
					geometry_cylinders=possibleCylinderAnz[cyl];
					geometry_heads=heads;
					geometry_sectors=possibleSectorAnz[secs];

					printf("Errechnete Image Geometrie: %d %d %d\n",geometry_cylinders,geometry_heads,geometry_sectors);
				}
			}
		}
	}

	setAttributesForEveryCylinder(geometry_sectors, 22, 0x4E);

	if (format==FLOPPY_FORMAT_UNKNOWN)
	{
		if (geometry_cylinders==80 && geometry_heads==2 && geometry_sectors==11)
		{
			format=FLOPPY_FORMAT_AMIGA_DD;
			printf("Wahrscheinlich Amiga DD...\n");
		}

		if (geometry_cylinders==80 && geometry_heads==2 && geometry_sectors==18)
		{
			format=FLOPPY_FORMAT_ISO_HD;
			printf("Wahrscheinlich ISO HD\n");
		}

		if (geometry_cylinders==80 && geometry_heads==2 && geometry_sectors==9)
		{
			format=FLOPPY_FORMAT_ISO_DD;
			printf("Wahrscheinlich ISO DD\n");
		}
	}

	FILE *f=fopen(path,"r");
	assert(f);

	int cylinderSize=geometry_heads * geometry_sectors * geometry_payloadBytesPerSector;
	printf("CylinderSize: %d %d %d %d\n",cylinderSize, geometry_heads, geometry_sectors, geometry_payloadBytesPerSector);
	if (format==FLOPPY_FORMAT_AMIGA_DD)
	{
		for (cyl=0; cyl < geometry_cylinders;cyl++)
		{
			//int bytes_read = fread(&image_cylinderBuf[cyl][head*geometry_sectors*geometry_payloadBytesPerSector],1,cylinderSize,f);
			int bytes_read = fread(&image_cylinderBuf[cyl][0],1,cylinderSize,f);
			assert(cylinderSize == bytes_read);

			image_cylinderSize[cyl]=geometry_sectors * geometry_heads * geometry_payloadBytesPerSector;
			geometry_sectorsPerCylinder[cyl]=geometry_sectors;
		}
	}
	else //für ISO
	{
		if (geometry_sectors==11)
			geometry_iso_sectorInterleave=1;
		else
			geometry_iso_sectorInterleave=0;

		floppy_iso_buildSectorInterleavingLut(geometry_sectors);
		floppy_iso_evaluateSectorInterleaving(cyl,geometry_sectors);

		//printf("CylinderSize: %d %d %d %d\n",cylinderSize, geometry_heads, geometry_sectors, geometry_payloadBytesPerSector);
		for (cyl=0; cyl < geometry_cylinders;cyl++)
		{
			geometry_sectorsPerCylinder[cyl]=geometry_sectors;
			geometry_iso_gap3length[cyl]=22;
			geometry_iso_fillerByte[cyl]=0x4E;
			image_cylinderSize[cyl]=geometry_sectors * geometry_heads * geometry_payloadBytesPerSector;

			for (head=0; head < geometry_heads;head++)
			{

				for(i=1;i <= geometry_sectors;i++)
				{
					int sectorPos=floppy_iso_getSectorPos(cyl,i);
					assert(sectorPos>=0);

					geometry_iso_sectorHeaderSize[cyl][sectorPos]=2;
					geometry_iso_sectorErased[cyl][sectorPos]=0;
					geometry_actualSectorSize[cyl][sectorPos]=geometry_payloadBytesPerSector;

					int dataPos=sectorPos*geometry_payloadBytesPerSector+head*geometry_sectors * geometry_payloadBytesPerSector;

					int bytes_read = fread(&image_cylinderBuf[cyl][dataPos],1,geometry_payloadBytesPerSector,f);
					assert(geometry_payloadBytesPerSector == bytes_read);

					printf("sec %d %d %x pos %x\n",cyl,i,sectorPos,dataPos);
				}
			}
		}
	}
	fclose(f);



	return 0;

}
