
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
#include <math.h>

#include "slamySTM32Floppy.h"




int saveCompressedVariableDensity(unsigned char *dest, CapsULong *timebuf, CapsULong timelen)
{
	int compressedBytes=0;
	CapsULong i;

	assert(timebuf);

	CapsULong currentVal=timebuf[0];
	CapsULong trueCellLength=MFM_BITTIME_DD*currentVal/2000;

	dest[0]=0;
	dest[1]=0;
	dest[2]=trueCellLength>>8;
	dest[3]=trueCellLength&0xff;
	dest+=4;
	compressedBytes+=4;
	printf("density from %d is %d\n",0,currentVal);

	for (i=1; i<timelen; i++)
	{
		if (currentVal!=timebuf[i])
		{
			currentVal=timebuf[i];

			CapsULong trueCellLength=MFM_BITTIME_DD*currentVal/2000;

			dest[0]=i>>8;
			dest[1]=i&0xff;
			dest[2]=trueCellLength>>8;
			dest[3]=trueCellLength&0xff;
			dest+=4;
			compressedBytes+=4;
			printf("density from %d is %d\n",i,currentVal);
		}
	}

	dest[0]=0xff;
	dest[1]=0xff;
	dest[2]=0;
	dest[3]=0;
	dest+=4;
	compressedBytes+=4;

	return compressedBytes;
}

int readImage_ipf(const char *path)
{

	if (CAPSInit() == imgeOk)
	{
		int i, id = CAPSAddImage();

		if (CAPSLockImage(id, (char*)path) == imgeOk)
		{
			struct CapsImageInfo cii;

			if (CAPSGetImageInfo(&cii, id) == imgeOk)
			{
				printf("Type: %d\n", (int)cii.type);
				printf("Release: %d\n", (int)cii.release);
				printf("Revision: %d\n", (int)cii.revision);
				printf("Min Cylinder: %d\n", (int)cii.mincylinder);
				printf("Max Cylinder: %d\n", (int)cii.maxcylinder);
				printf("Min Head: %d\n", (int)cii.minhead);
				printf("Max Head: %d\n", (int)cii.maxhead);
				printf("Creation Date: %04d/%02d/%02d %02d:%02d:%02d.%03d\n", (int)cii.crdt.year, (int)cii.crdt.month, (int)cii.crdt.day, (int)cii.crdt.hour, (int)cii.crdt.min, (int)cii.crdt.sec, (int)cii.crdt.tick);
				printf("Platforms:");
				for (i = 0; i < CAPS_MAXPLATFORM; i++)
					if (cii.platform[i] != ciipNA)
						printf(" %s", CAPSGetPlatformName(cii.platform[i]));
				printf("\n");
			}

			assert(cii.mincylinder == 0);
			assert(cii.minhead == 0);
			assert(cii.maxhead == 1);

			geometry_cylinders=0;
			geometry_heads=2;
			format=FLOPPY_FORMAT_RAW_MFM;
			struct CapsTrackInfo trackInf;

			int cylinder=0;
			int head=0;

			for (cylinder = cii.mincylinder ; cylinder <= cii.maxcylinder ; cylinder++ )
			{
				int cylBufIndex=0;

				for (head = cii.minhead; head <= cii.maxhead; head++)
				//for (head = (int)cii.maxhead; head >= (int)cii.minhead; head--)
				{
					//printf("CAPSLockTrack %d %d %d\n",head >= cii.minhead,cylinder,head);

					int ret=CAPSLockTrack(&trackInf, id, cylinder, head,DI_LOCK_INDEX | DI_LOCK_DENVAR);
					if (ret == imgeOk)
					{
						/*
						printf("trackInf\n");
						printf("  type:%d\n",trackInf.type);
						printf("  cylinder:%d\n",trackInf.cylinder);
						printf("  head:%d\n",trackInf.head);
						printf("  sectorcnt:%d\n",trackInf.sectorcnt);
						printf("  sectorsize:%d\n",trackInf.sectorsize);
						printf("  trackcnt:%d\n",trackInf.trackcnt);
						printf("  trackbuf:%p\n",trackInf.trackbuf);
						printf("  trackdata:%d\n",trackInf.trackdata[0]);
						printf("  tracksize:%d\n",trackInf.tracksize[0]);
						printf("  timelen:%d\n",trackInf.timelen);
						printf("  timebuf:%d\n",trackInf.timebuf);
						*/


						printf("trackInf %d %d %d %d %d %d %d %d\n",
								trackInf.type,
								trackInf.cylinder,
								trackInf.head,
								trackInf.sectorcnt,
								trackInf.sectorsize,
								trackInf.trackcnt,
								trackInf.tracksize[0],
								trackInf.timelen);



						int timeDataNeeded=0;
						int possibleCelllength=0;

						if (trackInf.type == ctitVar)
							timeDataNeeded=1;
						else
						{
							possibleCelllength=floor( ((double)CELL_TICKS_PER_ROTATION_300) / ((double)trackInf.tracksize[0] * 8.0));
							printf("possibleCelllength: %d %d\n",
									possibleCelllength,
									MFM_BITTIME_DD/2);

							if (possibleCelllength < MFM_BITTIME_DD/2)
								timeDataNeeded=1;
						}

						if (trackInf.type == ctitAuto || trackInf.type == ctitNoise || trackInf.type == ctitVar)
						{
							assert(trackInf.trackcnt < 2);
							if (trackInf.trackcnt==1)
							{

								if (trackInf.cylinder >= geometry_cylinders)
									geometry_cylinders=trackInf.cylinder+1;

								assert((trackInf.tracksize[0] + 3) < (CYLINDER_BUFFER_SIZE - cylBufIndex));

								//printf("tracksize @ index %d\n",cylBufIndex);

								image_cylinderBuf[cylinder][cylBufIndex]=trackInf.tracksize[0]>>8;
								image_cylinderBuf[cylinder][cylBufIndex+1]=trackInf.tracksize[0]&0xff;
								image_cylinderBuf[cylinder][cylBufIndex+2]=head | (timeDataNeeded ? 2 : 0 );

								memcpy(&image_cylinderBuf[cylinder][cylBufIndex+3],&trackInf.trackdata[0][0],trackInf.tracksize[0]);
								cylBufIndex+= 3 + trackInf.tracksize[0];
							}
						}
						else
						{
							//printf("Variable Data!\n");
							assert(0);
						}


						if (trackInf.type == ctitVar)
						{
							assert( trackInf.timelen > 0);
							assert( trackInf.timebuf);

							int anzBytes=saveCompressedVariableDensity(&image_cylinderBuf[cylinder][cylBufIndex+3],trackInf.timebuf,trackInf.timelen);
							assert(anzBytes < (CYLINDER_BUFFER_SIZE - cylBufIndex));

							image_cylinderBuf[cylinder][cylBufIndex]=anzBytes>>8;
							image_cylinderBuf[cylinder][cylBufIndex+1]=anzBytes&0xff;
							image_cylinderBuf[cylinder][cylBufIndex+2]=head | 4; //markiert variable density data
							cylBufIndex+= 3 + anzBytes;
						}
						else if (timeDataNeeded)
						{
							//normalerweise ist das hier gar nicht notwendig. Aber Turrican 1 und 2 verwenden Long Tracks. Die Bit Rate muss eventuell erhöht werden.


							//Header für Density Data
							image_cylinderBuf[cylinder][cylBufIndex+0]=0;
							image_cylinderBuf[cylinder][cylBufIndex+1]=8;
							image_cylinderBuf[cylinder][cylBufIndex+2]=head | 4; //markiert variable density data
							cylBufIndex+=3;

							//Variable Density Data

							image_cylinderBuf[cylinder][cylBufIndex+0]=0;
							image_cylinderBuf[cylinder][cylBufIndex+1]=0;
							image_cylinderBuf[cylinder][cylBufIndex+2]=possibleCelllength>>8;
							image_cylinderBuf[cylinder][cylBufIndex+3]=possibleCelllength&0xff;
							cylBufIndex+=4;

							image_cylinderBuf[cylinder][cylBufIndex+0]=0xff;
							image_cylinderBuf[cylinder][cylBufIndex+1]=0xff;
							image_cylinderBuf[cylinder][cylBufIndex+2]=0;
							image_cylinderBuf[cylinder][cylBufIndex+3]=0;
							cylBufIndex+=4;

						}

						if (cylinder==18)
						{
							for (int i=0; i< trackInf.tracksize[0]; i++)
							{

								if ((i%16)==0)
									printf("\n %06d ",i);

								printf("%02x ",trackInf.trackdata[0][i]);

							}
							printf("\n");
						}

						/*
						if (trackInf.trackdata[0])
						{
							printf("Trackdata %d %d: \n",cylinder,head);
							for (i=0; i< trackInf.tracksize[0]; i++)
							{
								printf("%02x ",trackInf.trackdata[0][i]);
								if ((i%16)==15)
									printf("\n");
							}
							printf("\n");
						}

						if (trackInf.type == ctitVar && trackInf.timebuf)
						{
							printf("Timedata %d %d: \n",cylinder,head);
							for (i=0; i< trackInf.timelen; i++)
							{
								printf("%02x ",trackInf.timebuf[i]);
								if ((i%16)==15)
									printf("\n");
							}
							printf("\n");
						}
						*/

						//exit(0);

						CAPSUnlockTrack(id,cylinder,head);
					}
					else
					{
						printf("CAPSLockTrack returned %d\n",ret);
						assert(0);
					}
				}
				//exit(0);

				//Kennzeichne das Ende
				image_cylinderBuf[cylinder][cylBufIndex]=0;
				image_cylinderBuf[cylinder][cylBufIndex+1]=0;
				image_cylinderBuf[cylinder][cylBufIndex+2]=0;
				cylBufIndex+= 3;

				image_cylinderSize[cylinder]=cylBufIndex;
				geometry_sectorsPerCylinder[cylinder]=1; //bei raw nehmen wir an, es gibt einen großen sector

				//printf("image_rawCylinderSize[cylinder] %d\n",image_cylinderSize[cylinder]);
			}

			CAPSUnlockImage(id);
		}
		CAPSRemImage(id);

		CAPSExit();
	}

	return 0;
}


