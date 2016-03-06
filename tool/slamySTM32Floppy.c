
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

uint32_t geometry_payloadBytesPerSector=512;
uint32_t geometry_cylinders=0;
uint32_t geometry_heads=0;
uint32_t geometry_sectors=0; //wenn 0, dann zählt geometry_sectorsPerCylinder
unsigned char geometry_sectorsPerCylinder[MAX_CYLINDERS];
unsigned char geometry_iso_sectorPos[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK];
uint32_t geometry_iso_cpcSectorIdMode=0;
int geometry_iso_sectorInterleave;
unsigned char image_cylinderBuf[MAX_CYLINDERS][MAX_HEADS * MAX_SECTORS_PER_TRACK * MAX_SECTOR_SIZE];

enum floppyFormat format;

void floppy_iso_buildSectorInterleavingLut();

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
	FLOPPY_FORMAT_ISO_DD,
	FLOPPY_FORMAT_ISO_HD,
	FLOPPY_FORMAT_AMIGA_DD
};


char *formatStr[]=
{
	"UNKNOWN",
	"Iso DD",
	"Iso HD",
	"Amiga DD"
};


void discoverFormat()
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
	sendFrame(sendBuf,7);

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
	sendBuf[6]=4; //Write Track
	sendBuf[7]=cylinder; //Welche Spur
	sendBuf[8]=geometry_heads;

	int tracksize;

	if (geometry_sectors) //global Sectoranzahl festgelegt ?
	{
		sendBuf[9]=geometry_sectors;
		tracksize=geometry_payloadBytesPerSector * geometry_heads * geometry_sectors;
	}
	else
	{
		sendBuf[9]=geometry_sectorsPerCylinder[cylinder];
		tracksize=geometry_payloadBytesPerSector * geometry_heads * (int)geometry_sectorsPerCylinder[cylinder];
	}

	//printf("tracksize %d   %d %d\n",tracksize,geometry_sectors,geometry_heads);
	memcpy(&sendBuf[10],geometry_iso_sectorPos[cylinder],18);
	
	sendFrame(sendBuf,10+18);

	/*
	printf("%d %d %d -> ",geometry_payloadBytesPerSector,geometry_heads,geometry_sectors);
	for (i=0;i<sendBuf[9];i++)
	{
		printf("%d ",sendBuf[10+i]);
	}
	printf("\n");
	*/

	memcpy(trackBuf,image_cylinderBuf[cylinder],tracksize);

	/*
	for (i=0;i<geometry_sectors*geometry_heads;i++)
	{
		printf("%x\n",trackBuf[i*geometry_payloadBytesPerSector]);
	}
	*/

	crc=0xffff;
	for(i=0;i<tracksize;i++)
	{
		crc_shiftByte(trackBuf[i]);
	}

	trackBuf[tracksize++]=crc>>8;
	trackBuf[tracksize++]=crc&0xff;

	printf("Cylinder write %d ... sending %d byte\n",cylinder,tracksize);
	sendMultipleFrames(trackBuf,tracksize);

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
	int tracksize=geometry_payloadBytesPerSector * geometry_heads * geometry_sectors;
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
	sendBuf[10]=geometry_sectors;
	sendBuf[11]=calibrate;
	sendBuf[12]=geometry_iso_cpcSectorIdMode;


	sendFrame(sendBuf,13);

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

int analyseImage(const char *path)
{
	struct stat info;

	int i;
	int cyl,head,heads,secs;
	int ret=-1;

	char *fileTypeStr=strrchr(path,'.');

	enum
	{
		BINARY,
		AMSTRAD_CPC
	} expectedFormat=BINARY;


	if (fileTypeStr)
	{
		if (!strcmp(fileTypeStr,".dsk"))
		{
			printf("Amstrad CPC - DSK Image\n");
			expectedFormat=AMSTRAD_CPC;
		}
		else if (!strcmp(fileTypeStr,".st"))
		{
			printf("Atari ST - Binary Image\n");
		}
		else if (!strcmp(fileTypeStr,".adf"))
		{
			printf("Amiga - Binary Image\n");
		}
	}
	else
		exit(1);

	if (expectedFormat==BINARY)
	{
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
						ret=0;
					}
				}
			}
		}

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

		return ret;
	}
	else if (expectedFormat==AMSTRAD_CPC)
	{
		unsigned char diskInfoBlock[256];
		unsigned char trackInfoBlock[256];

		geometry_iso_cpcSectorIdMode=1;

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
			//int tracksize=((int)temp[2]) | (((int)temp[3])<<8);
			printf("Number of Tracks:%d\n",tracks);
			printf("Number of Sides:%d\n",sides);
			//printf("Size of a Track:%d\n",tracksize);

			geometry_cylinders=tracks;
			geometry_heads=sides;
			geometry_sectors=0; //Sectoranzahl wird für jeden Cylinder festgelegt.

			int i,j;
			for (i=0;i<tracks*sides;i++)
			{
				//printf("Track %d -> Cyl:%d Head:%d Size %d\n",i,i>>1,i&1,diskInfoBlock[0x34+i]);
			}

			for (i=0;i<tracks*sides;i++)
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

				assert(sectorSize==2);
				//printf("Track %d %d %d %d\n",track,side,sectorSize,sectors);

				if (side==0)
					geometry_sectorsPerCylinder[track]=sectors;
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
					unsigned int actualLength=((int)trackInfoBlock[0x18+j*8+6]) | (((int)trackInfoBlock[0x18+j*8+7])<<8);


					printf("Sector %d %d %x %d %d\n",
							sectorheader_cylinder,
							sectorheader_side,
							sectorheader_sector,
							sectorheader_sectorsize,
							actualLength);

					assert(sectorheader_sectorsize==2);

					if ((sectorheader_sector & 0xf0) != 0xC0)
					{
						printf("Bei dieser Amstrad CPC Disk ist das Upper Nibble des Sektors nicht 0xC. Das geht aktuell nicht!\n");
						exit(1);
					}

					if (side==0)
					{
						geometry_iso_sectorPos[track][j]=(sectorheader_sector&0xf);
						printf("geometry_iso_sectorPos [%d][%d] = %d\n",track,j,(sectorheader_sector&0xf));
					}
					else
					{
						if (geometry_iso_sectorPos[track][j]!=(sectorheader_sector&0xf))
						{
							printf("Bei dieser Amstrad CPC Disk unterscheiden sich die Tracks eines Cylinders anhand des Sector Interleavings.\nDas geht gerade nicht!\n");
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
					int sectorPos=geometry_iso_sectorPos[track][j]-1;
					bytes_read=fread(&image_cylinderBuf[track][geometry_payloadBytesPerSector*(sectorPos+sectors*side)],1,512,f);
					assert(bytes_read==512);
					//printf("1. Byte %x\n",image_cylinderBuf[track][geometry_payloadBytesPerSector*(sectorPos+sectors*side)]);
				}
			}

			fclose(f);
			return FLOPPY_FORMAT_ISO_DD;
		}
		else
			assert(0);
	}
}

void floppy_iso_standardSectorPositions()
{
	int i;
	for (i=0;i<MAX_SECTORS_PER_TRACK;i++)
		geometry_iso_sectorPos[0][i]=i+1;

	for (i=1;i<MAX_CYLINDERS;i++)
	{
		memcpy(geometry_iso_sectorPos[i],geometry_iso_sectorPos[0],MAX_SECTORS_PER_TRACK);
	}
}

void floppy_iso_buildSectorInterleavingLut()
{
	int i;
	int sector=1;
	int placePos=0;

	printf("floppy_iso_buildSectorInterleavingLut %d %d\n",geometry_cylinders,geometry_sectors);

	for (i=0;i<geometry_sectors;i++)
		geometry_iso_sectorPos[0][i]=0;

	while (sector <= geometry_sectors)
	{
		geometry_iso_sectorPos[0][placePos]=sector;
		placePos+=(geometry_iso_sectorInterleave+1);
		if (placePos >= geometry_sectors)
		{
			placePos-=geometry_sectors;
		}

		while (geometry_iso_sectorPos[0][placePos])
			placePos++;

		sector++;
	}

	for (i=1;i<geometry_cylinders;i++)
	{
		memcpy(geometry_iso_sectorPos[i],geometry_iso_sectorPos[0],MAX_SECTORS_PER_TRACK);
	}
}


void parseFormatString(char *str)
{

	if (!strncmp(str,"amiga",5))
	{
		geometry_cylinders=80;
		geometry_heads=2;
		geometry_sectors=11;
		format=FLOPPY_FORMAT_AMIGA_DD;
	}
	else if (!strncmp(str,"isodd",5))
	{
		geometry_cylinders=80;
		geometry_heads=2;
		geometry_sectors=9;
		format=FLOPPY_FORMAT_ISO_DD;
	}
	else if (!strncmp(str,"isohd",5))
	{
		geometry_cylinders=80;
		geometry_heads=2;
		geometry_sectors=18;
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
			geometry_sectors=atoi(str);
		}
	}
}


int floppy_iso_getSectorNum(int cyl,int sectorPos)
{
	//return ((sectorPos-1)*(geometry_iso_sectorInterleave+1) % geometry_sectors) +1;
	return geometry_iso_sectorPos[cyl][sectorPos-1];
}

void floppy_iso_evaluateSectorInterleaving(int cyl)
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
			if (expectedSector==geometry_iso_sectorPos[cyl][sectorpos])
			{
				printf("%2d ",geometry_iso_sectorPos[cyl][sectorpos]);
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
		printf("  read  amiga <path>    Schreibt den Disketteninhalt in ein Image\n");
		printf("        isodd <path>\n");
		printf("        isohd <path>\n");
		printf("  write       <path>   Schreibt das Image auf Diskette\n");
		printf("        isodd <path>   Erwingt ISO DD\n");
		printf("  discover             Versucht das Diskettenformat zu erkennen\n");
		printf("  info <path>          Analysiert die Geometrie des Images\n");

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
		geometry_sectors=atoi(argv[3]);
		floppy_iso_buildSectorInterleavingLut(atoi(argv[2]));
		floppy_iso_evaluateSectorInterleaving(0);

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
		int diskSize=geometry_cylinders*geometry_heads*geometry_sectors*geometry_payloadBytesPerSector;

		f=fopen(filename,"wb");
		assert(f);

		readDisk(0,geometry_cylinders-1);

		int cylinderSize=geometry_heads * geometry_sectors * geometry_payloadBytesPerSector;
		int cyl;
		for (cyl=0; cyl < geometry_cylinders;cyl++)
		{
			int bytes_written = fwrite(image_cylinderBuf[cyl],1,cylinderSize,f);
			assert(cylinderSize == bytes_written);
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

		configureController(CALIBRATE_MAGIC);

		writeDisk(0,geometry_cylinders-1);

		printf("Image erfolgreich auf Diskette geschrieben!\n");
	}
	else if (!strcmp(argv[1],"discover") && argc == 2)
	{
		discoverFormat();
	}
	


	return 0;
}


