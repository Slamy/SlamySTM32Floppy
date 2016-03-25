
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

#define BULK_EP_OUT     0x81
#define BULK_EP_IN      0x01

struct termios oldkey, newkey;
int tty;

libusb_device_handle *devicehandle;
unsigned short crc=0xFFFF;

#define MAX_SECTOR_SIZE 512
#define MAX_CYLINDERS 85
#define MAX_HEADS 2
#define MAX_SECTORS_PER_TRACK 18


uint32_t geometry_cylinders=0;
uint32_t geometry_heads=0;
unsigned char geometry_sectorsPerCylinder[MAX_CYLINDERS];
unsigned char geometry_iso_gap3length[MAX_CYLINDERS];
unsigned char geometry_iso_fillerByte[MAX_CYLINDERS];
unsigned short geometry_actualSectorSize[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK]; //tatsächliche Größe der Daten in Byte
unsigned char geometry_iso_sectorId[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK]; //Interleaving ist damit auch abgedeckt
unsigned char geometry_iso_sectorHeaderSize[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK]; //z.B. 2 für 512 Byte Sektoren. nur untere 3 bits werden benutzt
unsigned char geometry_iso_sectorErased[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK];



int geometry_iso_sectorInterleave;

//#define CYLINDER_BUFFER_SIZE MAX_HEADS * MAX_SECTORS_PER_TRACK * MAX_SECTOR_SIZE //für HD ausreichend
#define CYLINDER_BUFFER_SIZE (14000 * 2) //basierend auf Turrican2.ipf

unsigned char image_cylinderBuf[MAX_CYLINDERS][CYLINDER_BUFFER_SIZE];
unsigned int image_cylinderSize[MAX_CYLINDERS];


enum floppyFormat format;

void floppy_iso_buildSectorInterleavingLut();
void setSectorAnzForEveryCylinder(int geometry_sectors);

static const unsigned int isoSectorSizes[]={
		128, //0
		256, //1
		512, //2
		1024, //3
		2048, //4
		4096, //5
		8192 //6
};

void crc_shiftByte(unsigned char b)
{
	int i;
	for (i = 0; i < 8; i++)
		crc = (crc << 1) ^ ((((crc >> 8) ^ (b << i)) & 0x0080) ? 0x1021 : 0);
}

void initUsb()
{
	int err;

	err = libusb_init(NULL);
	if (err < 0)
	{
		printf("Libusb lässt sich nicht initialisieren! :-(\n");
		exit(1);
	}

	devicehandle=libusb_open_device_with_vid_pid(NULL,0x0483,0x5740);
	if (!devicehandle)
	{
		printf("Slamy's USB Floppy konnte nicht gefunden werden! :-(\n");
		exit(1);
	}

	libusb_set_debug(NULL,3);

	err=libusb_claim_interface(devicehandle,0);

	switch (err)
	{
		case 0: break;
		case LIBUSB_ERROR_NOT_FOUND: printf("LIBUSB_ERROR_NOT_FOUND\n");exit(1);
		case LIBUSB_ERROR_NO_DEVICE: printf("LIBUSB_ERROR_NO_DEVICE\n");exit(1);
		default: printf("Unknown Error %d\n",err);exit(1);
	}
}


int expect_floppy_readTrack(unsigned char *buf, int len)
{

	int totalReceivedBytes=0;
	int bytesReceived=0;
	int err;

	while (totalReceivedBytes < len)
	{
		err=libusb_bulk_transfer(devicehandle,BULK_EP_OUT,buf+totalReceivedBytes,64,&bytesReceived,0);  //64 : Max Packet Lenghth

        if(err == 0)
        {
        	if (bytesReceived==64)
        	{
        		totalReceivedBytes+=bytesReceived;
        		//printf("Received %d Byte\n",bytesReceived);
        	}
        	else if (bytesReceived==4)
        	{
        		printf("floppy_readTrack() returned %d\n",buf[totalReceivedBytes+3]);
        		return buf[totalReceivedBytes+3];
        	}
        	else
        	{
        		printf("Unerwartete Antwort von floppy_readTrack(): %d\n",bytesReceived);
        		return 20;
        	}
        }
        else
        {
            printf("Error in read!");
            exit(1);
        }
	}

	return 0;
}


int receiveFrame(unsigned char *buf)
{
	int err;
	int bytesReceived=0;
	err=libusb_bulk_transfer(devicehandle,BULK_EP_OUT,buf,64,&bytesReceived,0);  //64 : Max Packet Lenghth

	if(err == 0)
	{
		//printf("Received %d Byte\n",bytesReceived);
	}
	else
	{
		printf("Error in read!\n");
		exit(1);
	}
	return bytesReceived;
}


void sendFrame(unsigned char *buf, int len)
{
	int err;
	int bytesSent=0;
	err=libusb_bulk_transfer(devicehandle,BULK_EP_IN,buf,len,&bytesSent,0);  //64 : Max Packet Lenghth

	if(err == 0)
	{
		//printf("Sent %d Byte\n",bytesSent);
	}
	else
	{
		printf("Error in read!");
		exit(1);
	}
}

void sendMultipleFrames(unsigned char *buf, int len)
{
	while (len)
	{
		if (len>=64)
		{
			sendFrame(buf,64);
			len-=64;
			buf+=64;
		}
		else
		{
			sendFrame(buf,len);
			len=0;
		}
	}
}

enum floppyFormat
{
	FLOPPY_FORMAT_UNKNOWN,

	/* Discoverable formats */
	FLOPPY_FORMAT_ISO_DD,
	FLOPPY_FORMAT_ISO_HD,
	FLOPPY_FORMAT_AMIGA_DD,

	/* Special formats */
	FLOPPY_FORMAT_RAW
};


char *formatStr[]=
{
	"UNKNOWN",
	"Iso DD",
	"Iso HD",
	"Amiga DD",
	"Raw MFM"
};


void discoverFormat(int cylinder, int head)
{
	int bytes_read;
	unsigned char sendBuf[64];
	unsigned char recvBuf[64];

	sendBuf[0]='F'; //Magic Number
	sendBuf[1]='l';
	sendBuf[2]='o';
	sendBuf[3]='p';
	sendBuf[4]='p';
	sendBuf[5]='y';
	sendBuf[6]=2; //Discover Format
	sendBuf[7]=cylinder; //Discover Format
	sendBuf[8]=head; //Discover Format
	sendFrame(sendBuf,9);

	bytes_read=receiveFrame(recvBuf);
	if (bytes_read!=5)
	{
		printf("Unexpected answer!\n");
		return;
	}

	if (memcmp("FMT",recvBuf,3))
	{
		printf("Unexpected answer 2!\n");
		return;
	}

	if (recvBuf[3] > 3)
	{
		printf("Format ID: %d mit %d Sektoren\n",recvBuf[3],recvBuf[4]);
	}
	else
	{
		printf("Format: %s mit %d Sektoren\n",formatStr[recvBuf[3]],recvBuf[4]);
	}
}

void writeCylinderFromImage(int cylinder)
{
	int i;
	unsigned char cylinderBuf[CYLINDER_BUFFER_SIZE];
	int bytes_read;
	int total_bytes_read;
	unsigned char sendBuf[64];
	unsigned char recvBuf[64];
	int tracksize=0;


	/*
	printf("%d %d %d -> ",geometry_payloadBytesPerSector,geometry_heads,geometry_sectors);
	for (i=0;i<sendBuf[9];i++)
	{
		printf("%d ",sendBuf[10+i]);
	}
	printf("\n");
	*/

	if (format==FLOPPY_FORMAT_ISO_DD || format==FLOPPY_FORMAT_ISO_HD)
	{
		for(i=0;i<geometry_sectorsPerCylinder[cylinder];i++)
		{
			cylinderBuf[tracksize++]=geometry_iso_sectorId[cylinder][i];
		}

		for(i=0;i<geometry_sectorsPerCylinder[cylinder];i++)
		{
			cylinderBuf[tracksize]=geometry_iso_sectorHeaderSize[cylinder][i];
			printf("writeCylinderFromImage %d %d\n",i,geometry_iso_sectorErased[cylinder][i]);
			if (geometry_iso_sectorErased[cylinder][i])
			{
				cylinderBuf[tracksize]|=0x80;
			}
			tracksize++;
		}

		for(i=0;i<geometry_sectorsPerCylinder[cylinder];i++)
		{
			cylinderBuf[tracksize++]=(geometry_actualSectorSize[cylinder][i]>>8)&0xff;
			cylinderBuf[tracksize++]=geometry_actualSectorSize[cylinder][i]&0xff;
		}
	}


	memcpy(&cylinderBuf[tracksize],image_cylinderBuf[cylinder],image_cylinderSize[cylinder]);
	tracksize+=image_cylinderSize[cylinder];
	assert(tracksize < sizeof(cylinderBuf));

	sendBuf[0]='F'; //Magic Number
	sendBuf[1]='l';
	sendBuf[2]='o';
	sendBuf[3]='p';
	sendBuf[4]='p';
	sendBuf[5]='y';
	sendBuf[6]=4; //Write Track
	sendBuf[7]=cylinder; //Welche Spur
	sendBuf[8]=geometry_sectorsPerCylinder[cylinder];
	sendBuf[9]=(tracksize>>8)&0xff;
	sendBuf[10]=tracksize&0xff;
	sendBuf[11]=geometry_iso_gap3length[cylinder];
	sendBuf[12]=geometry_iso_fillerByte[cylinder];

	sendFrame(sendBuf,13);

	/*
	for (i=0;i<geometry_sectors*geometry_heads;i++)
	{
		printf("%x\n",trackBuf[i*geometry_payloadBytesPerSector]);
	}
	*/

	crc=0xffff;
	for(i=0;i<tracksize;i++)
	{
		crc_shiftByte(cylinderBuf[i]);
	}

	cylinderBuf[tracksize++]=crc>>8;
	cylinderBuf[tracksize++]=crc&0xff;

	printf("Cylinder write %d ... sending %d byte\n",cylinder,tracksize);
	sendMultipleFrames(cylinderBuf,tracksize);

	bytes_read=receiveFrame(recvBuf);
	if (bytes_read!=4)
	{
		printf("Unexpected answer!\n");
		return;
	}

	if (memcmp("WCR",recvBuf,3))
	{
		printf("Unexpected answer 2!\n");
		return;
	}

	if (recvBuf[3] != 0)
	{
		printf("writeTrackFromImage error code %d\n",recvBuf[3]);
		exit(1);
	}

}

void readCylinderToImage(int cylinder)
{
	int i;
	unsigned char trackBuf[512*18*2];
	int bytes_read;
	int total_bytes_read;
	unsigned char sendBuf[64];
	unsigned char recvBuf[64];

	sendBuf[0]='F'; //Magic Number
	sendBuf[1]='l';
	sendBuf[2]='o';
	sendBuf[3]='p';
	sendBuf[4]='p';
	sendBuf[5]='y';
	sendBuf[6]=1; //Read Track
	sendBuf[7]=cylinder; //Welche Spur
	sendFrame(sendBuf,8);

	memset(recvBuf,0,sizeof(recvBuf));

	bytes_read=receiveFrame(recvBuf);
	if (memcmp(sendBuf,recvBuf,8))
	{
		printf("Track read not accepted!\n");
		exit(1);
	}


	total_bytes_read=0;
	
	unsigned char checksum=0;
	int tracksize=image_cylinderSize[cylinder];
	//printf("%d %d %d\n",geometry_payloadBytesPerSector,geometry_heads,geometry_sectors);
	printf("Cylinder read %d ... expecting %d byte\n",cylinder,tracksize);

	int ret = expect_floppy_readTrack(trackBuf,tracksize);
	if (ret)
		exit(1);

	assert(receiveFrame(recvBuf)==2);

	//expectFixedDataSize(bytebuf,sendfilesize);
	
	crc=0xffff;

	for(i=0;i<tracksize;i++)
	{
		crc_shiftByte(trackBuf[i]);
	}
	
	crc_shiftByte(recvBuf[0]);
	crc_shiftByte(recvBuf[1]);

	assert(crc==0);

	memcpy(&image_cylinderBuf[cylinder],trackBuf,tracksize);
}

#define CALIBRATE_MAGIC 0x12

void configureController(unsigned char calibrate)
{
	int bytes_read;
	unsigned char sendBuf[64];
	unsigned char recvBuf[64];

	sendBuf[0]='F'; //Magic Number
	sendBuf[1]='l';
	sendBuf[2]='o';
	sendBuf[3]='p';
	sendBuf[4]='p';
	sendBuf[5]='y';
	sendBuf[6]=3; //Configure
	sendBuf[7]=format;
	sendBuf[8]=geometry_cylinders;
	sendBuf[9]=geometry_heads;
	sendBuf[10]=calibrate;


	sendFrame(sendBuf,12);

	bytes_read=receiveFrame(recvBuf);
	if (bytes_read!=2)
	{
		printf("Unexpected answer!\n");
		return;
	}

	if (memcmp("OK",recvBuf,2))
	{
		printf("Unexpected answer 2!\n");
		return;
	}
	printf("configureController finished!\n");
}

void readDisk(int first, int last)
{
	int track;

	for (track=first; track <= last; track++)
	{
		readCylinderToImage(track);
	}
}

void writeDisk(int first, int last)
{
	int track;

	for (track=first; track <= last; track++)
	{
		writeCylinderFromImage(track);
	}
}

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
	setSectorAnzForEveryCylinder(geometry_sectors);

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

	FILE *f=fopen(path,"r");
	assert(f);
	int cylinderSize=geometry_heads * geometry_sectors * geometry_payloadBytesPerSector;
	printf("CylinderSize: %d %d %d %d\n",cylinderSize, geometry_heads, geometry_sectors, geometry_payloadBytesPerSector);
	for (cyl=0; cyl < geometry_cylinders;cyl++)
	{
		//for (head=0; head < geometry_heads;head++)
		{
			//int bytes_read = fread(&image_cylinderBuf[cyl][head*geometry_sectors*geometry_payloadBytesPerSector],1,cylinderSize,f);
			int bytes_read = fread(&image_cylinderBuf[cyl][0],1,cylinderSize,f);
			assert(cylinderSize == bytes_read);

			/*
			for(i=0;i<geometry_sectors*geometry_heads;i++)
			{
				printf("sec %d %d %x pos %x\n",cyl,i,image_cylinderBuf[cyl][i*geometry_payloadBytesPerSector],i*geometry_payloadBytesPerSector);
			}
			*/
		}
	}
	fclose(f);

	floppy_iso_buildSectorInterleavingLut();

	return 0;

}

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


int saveCompressedVariableDensity(unsigned char *dest, CapsULong *timebuf, CapsULong timelen)
{
	int compressedBytes=0;
	CapsULong i;

	assert(timebuf);

	CapsULong currentVal=timebuf[0];

	dest[0]=0;
	dest[1]=0;
	dest[2]=currentVal>>8;
	dest[3]=currentVal&0xff;
	dest+=4;
	compressedBytes+=4;
	printf("density from %d is %d\n",0,currentVal);

	for (i=1; i<timelen; i++)
	{
		if (currentVal!=timebuf[i])
		{
			currentVal=timebuf[i];
			dest[0]=i>>8;
			dest[1]=i&0xff;
			dest[2]=currentVal>>8;
			dest[3]=currentVal&0xff;
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

	return compressedBytes;
}

int readImage_ipf(const char *path)
{

	if (CAPSInit() == imgeOk)
	{
		int i, id = CAPSAddImage();

		if (CAPSLockImage(id, path) == imgeOk)
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
			format=FLOPPY_FORMAT_RAW;
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
								image_cylinderBuf[cylinder][cylBufIndex+2]=head | (trackInf.type == ctitVar ? 2 : 0 );

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
				//printf("image_rawCylinderSize[cylinder] %d\n",image_cylinderSize[cylinder]);
			}

			CAPSUnlockImage(id);
		}
		CAPSRemImage(id);

		CAPSExit();
	}

	return 0;
}

int analyseImage(const char *path)
{

	int i;
	int cyl,head,heads,secs;

	char *fileTypeStr=strrchr(path,'.');


	if (fileTypeStr)
	{
		if (!strcmp(fileTypeStr,".dsk"))
		{
			printf("Amstrad CPC - DSK Image\n");
			return readImage_amstradCpc(path);
		}
		else if (!strcmp(fileTypeStr,".st"))
		{
			printf("Atari ST - Binary Image\n");
			return readImage_noHeader(path);
		}
		else if (!strcmp(fileTypeStr,".adf"))
		{
			printf("Amiga - Binary Image\n");
			return readImage_noHeader(path);
		}
		else if (!strcmp(fileTypeStr,".ipf"))
		{
			printf("Interchangable Preservation Format\n");
			return readImage_ipf(path);
		}
	}
	else
		exit(1);

	return 1;
}

void floppy_iso_standardSectorIds()
{
	int i;
	for (i=0;i<MAX_SECTORS_PER_TRACK;i++)
	{
		geometry_iso_sectorId[0][i]=i+1;
		geometry_iso_sectorHeaderSize[0][i]=2;
		geometry_actualSectorSize[0][i]=512;
	}

	for (i=1;i<MAX_CYLINDERS;i++)
	{
		memcpy(geometry_iso_sectorId[i],geometry_iso_sectorId[0],MAX_SECTORS_PER_TRACK);
		memcpy(geometry_iso_sectorHeaderSize[i],geometry_iso_sectorHeaderSize[0],MAX_SECTORS_PER_TRACK);
		memcpy(geometry_actualSectorSize[i],geometry_actualSectorSize[0],MAX_SECTORS_PER_TRACK * sizeof(unsigned short));
	}
}

void floppy_iso_buildSectorInterleavingLut(int geometry_sectors)
{
	int i;
	int sector=1;
	int placePos=0;

	printf("floppy_iso_buildSectorInterleavingLut %d %d\n",geometry_cylinders,geometry_sectors);

	for (i=0;i<geometry_sectors;i++)
	{
		geometry_iso_sectorId[0][i]=0;
	}

	while (sector <= geometry_sectors)
	{
		geometry_iso_sectorId[0][placePos]=sector;
		placePos+=(geometry_iso_sectorInterleave+1);
		if (placePos >= geometry_sectors)
		{
			placePos-=geometry_sectors;
		}

		while (geometry_iso_sectorId[0][placePos])
			placePos++;

		sector++;
	}

	for (i=1;i<geometry_cylinders;i++)
	{
		memcpy(geometry_iso_sectorId[i],geometry_iso_sectorId[0],MAX_SECTORS_PER_TRACK);
	}
}

void setSectorAnzForEveryCylinder(int geometry_sectors)
{
	int i;
	for (i=1;i<geometry_cylinders;i++)
	{
		geometry_sectorsPerCylinder[i]=geometry_sectors;
	}
}

void parseFormatString(char *str)
{

	if (!strncmp(str,"amiga",5))
	{
		geometry_cylinders=80;
		geometry_heads=2;
		setSectorAnzForEveryCylinder(11);
		format=FLOPPY_FORMAT_AMIGA_DD;
	}
	else if (!strncmp(str,"isodd",5))
	{
		geometry_cylinders=80;
		geometry_heads=2;
		setSectorAnzForEveryCylinder(9);
		format=FLOPPY_FORMAT_ISO_DD;
	}
	else if (!strncmp(str,"isohd",5))
	{
		geometry_cylinders=80;
		geometry_heads=2;
		setSectorAnzForEveryCylinder(18);
		format=FLOPPY_FORMAT_ISO_HD;
	}
	else
	{
		printf("Kein Format angegeben!\n");
		exit(0);
	}

	while ((str=strchr(str,'_'))!=NULL)
	{
		str++;
		if (*str=='i')
		{
			str++;

			geometry_iso_sectorInterleave=*str-'0';
		}
		else if (*str=='c')
		{
			str++;
			geometry_cylinders=atoi(str);
		}
		else if (*str=='s')
		{
			str++;
			setSectorAnzForEveryCylinder(atoi(str));
		}
	}
}


int floppy_iso_getSectorNum(int cyl,int sectorPos)
{
	//return ((sectorPos-1)*(geometry_iso_sectorInterleave+1) % geometry_sectors) +1;
	return geometry_iso_sectorId[cyl][sectorPos-1];
}

void floppy_iso_evaluateSectorInterleaving(int cyl, int geometry_sectors)
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
			if (expectedSector==geometry_iso_sectorId[cyl][sectorpos])
			{
				printf("%2d ",geometry_iso_sectorId[cyl][sectorpos]);
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

int main (int argc, char **argv)
{
	char *filename;
	FILE *f;

	if (argc<2)
	{
		printf("--- Slamy Floppy USB Tool ---\n");
		printf("  read  amiga <path>   Schreibt den Disketteninhalt in ein ADF-Image\n");
		printf("        isodd <path>   Schreibt den Disketteninhalt in ein ST-Image\n");
		printf("        isohd <path>   \n");
		printf("  write       <path>   Schreibt das Image auf Diskette\n");
		printf("                       Direkt erkannt werden\n");
		printf("                       ADF - Amiga Disk File für Amiga DOS 1.0 Disks\n");
		printf("                       ST - Atari ST Standard ISO\n");
		printf("                       DSK - Amstrad CPC Extended\n");
		printf("                       IPF - Interchangable Preservation Format\n");
		printf("  write isodd <path>   Erwingt ISO DD\n");
		printf("  write isohd <path>   Erwingt ISO HD\n");
		printf("  discover             Versucht das Diskettenformat zu erkennen\n");
		printf("  info <path>          Analysiert die Geometrie des Images\n");
		printf("\nDieses Tool hat Super-Floppy-Kräfte :-O!\n");


		return 0;
	}

	//floppy_iso_standardSectorPositions();

	if (!strcmp(argv[1],"info") && argc == 3)
	{
		analyseImage(argv[2]);
		return 0;
	}
	else if (!strcmp(argv[1],"interleave") && argc == 4)
	{
		int geometry_sectors=atoi(argv[3]);
		floppy_iso_buildSectorInterleavingLut(atoi(argv[2]));
		floppy_iso_evaluateSectorInterleaving(0,geometry_sectors);

		return 0;
	}
	else if (!strcmp(argv[1],"floppy") && argc == 2)
	{
		printf(" -------------------\n");
		printf("||_|                |\n");
		printf("||#|                |\n");
		printf("|        ___        |\n");
		printf("|       / O \\       |\n");
		printf("|       \\ ° /       |\n");
		printf("|        ---        |\n");
		printf("|                   |\n");
		printf("|    ___________    |\n");
		printf("|   |       __  |   |\n");
		printf("|   |      |  | |   |\n");
		printf("|   |      |  | |   |\n");
		printf("|   |      |__| |   |\n");
		printf(" -------------------\n");

		return 0;
	}

	initUsb();
	
	if (!strcmp(argv[1],"read") && argc == 4)
	{
		filename=argv[3];

		parseFormatString(argv[2]);

		/*
		geometry_cylinders=82;
		geometry_heads=2;
		geometry_sectors=11;
		*/

		configureController(0);

		f=fopen(filename,"wb");
		assert(f);

		readDisk(0,geometry_cylinders-1);

		int cyl;
		for (cyl=0; cyl < geometry_cylinders;cyl++)
		{
			int bytes_written = fwrite(image_cylinderBuf[cyl],1,image_cylinderSize[cyl],f);
			assert(image_cylinderSize[cyl] == bytes_written);
		}

		fclose(f);
		printf("Disk ausgelesen!\n");
	}
	else if (!strcmp(argv[1],"write") && argc >= 3)
	{

		if (argc==4)
		{
			filename=argv[3];
			parseFormatString(argv[2]);

		}
		else
		{
			filename=argv[2];

		}
		analyseImage(filename);

		printf("analyzed...\n");
		configureController(CALIBRATE_MAGIC);

		writeDisk(0,geometry_cylinders-1);

		printf("Image erfolgreich auf Diskette geschrieben!\n");
	}
	else if (!strcmp(argv[1],"discover") && argc == 4)
	{
		discoverFormat(atoi(argv[2]),atoi(argv[3]));
	}
	


	return 0;
}


