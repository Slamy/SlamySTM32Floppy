
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

uint32_t geometry_payloadBytesPerSector=512;
uint32_t geometry_cylinders=0;
uint32_t geometry_heads=0;
uint32_t geometry_sectors=0;
uint32_t geometry_iso_sectorInterleave=0;

enum floppyFormat format;

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


const char * const formatStr[]=
{
	"Unknown",
	"Iso DD, 9 Sektoren",
	"Iso HD, 18 Sektoren",
	"Amiga DD, 11 Sektoren"
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
	if (bytes_read!=4)
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
		printf("Format ID: %d\n",recvBuf[3]);
	}
	else
	{
		printf("Format: %s\n",formatStr[recvBuf[3]]);
	}
}

void writeCylinderFromImage(FILE *f,int cylinder)
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
	sendFrame(sendBuf,8);

	int tracksize=geometry_payloadBytesPerSector * geometry_heads * geometry_sectors;

	bytes_read=fread(trackBuf,1,tracksize,f);
	assert(tracksize==tracksize);

	crc=0xffff;
	for(i=0;i<tracksize;i++)
	{
		crc_shiftByte(trackBuf[i]);
	}

	trackBuf[tracksize++]=crc>>8;
	trackBuf[tracksize++]=crc&0xff;

	printf("Cylinder write %d ... sending %d byte\n",cylinder,tracksize);
	sendMultipleFrames(trackBuf,tracksize);

	printf("Expecting answer...\n");

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

void readCylinderToImage(FILE *f,int cylinder)
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

	int bytes_written=fwrite(trackBuf,1,tracksize,f);
	assert(tracksize==tracksize);
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
	sendBuf[12]=geometry_iso_sectorInterleave;

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

void readDisk(FILE *f, int first, int last)
{
	int track;

	for (track=first; track <= last; track++)
	{
		readCylinderToImage(f,track);
	}
}

void writeDisk(FILE *f, int first, int last)
{
	int track;

	for (track=first; track <= last; track++)
	{
		writeCylinderFromImage(f,track);
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

int calcGeometry(const char *path)
{
	struct stat info;
	assert(!stat(path, &info));

	int cyl,heads,secs;
	int ret=-1;

	for (cyl=0; cyl<sizeof(possibleCylinderAnz);cyl++)
	{
		for (heads=1; heads <=2; heads++)
		{
			for (secs=0; secs < sizeof(possibleSectorAnz); secs++)
			{
				//printf("%d %d\n",info.st_size,possibleCylinderAnz[cyl] * heads * possibleSectorAnz[secs] * geometry_payloadBytesPerSector);
				if (info.st_size == possibleCylinderAnz[cyl] * heads * possibleSectorAnz[secs] * geometry_payloadBytesPerSector)
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

	return ret;
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
	str=strchr(str,'_');
	if (str)
	{
		str++;
		if (*str=='i')
		{
			str++;
			geometry_iso_sectorInterleave=*str-'0';
		}
	}
}

uint8_t floppy_iso_sectorInterleave[18];

void floppy_iso_buildSectorInterleavingLut()
{
	int i;
	int sector=1;
	int placePos=0;

	for (i=0;i<sizeof(floppy_iso_sectorInterleave);i++)
		floppy_iso_sectorInterleave[i]=0;

	while (sector <= geometry_sectors)
	{
		floppy_iso_sectorInterleave[placePos]=sector;
		placePos+=(geometry_iso_sectorInterleave+1);
		if (placePos >= geometry_sectors)
		{
			placePos-=geometry_sectors;
		}

		while (floppy_iso_sectorInterleave[placePos])
			placePos++;

		sector++;
	}
}

int floppy_iso_getSectorNum(int sectorPos)
{
	//return ((sectorPos-1)*(geometry_iso_sectorInterleave+1) % geometry_sectors) +1;
	return floppy_iso_sectorInterleave[sectorPos-1];
}

void floppy_iso_evaluateSectorInterleaving()
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
			if (expectedSector==floppy_iso_getSectorNum(sectorpos))
			{
				printf("%2d ",floppy_iso_getSectorNum(sectorpos));
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

	if (!strcmp(argv[1],"info") && argc == 3)
	{
		calcGeometry(argv[2]);
		return 0;
	}
	else if (!strcmp(argv[1],"interleave") && argc == 4)
	{
		geometry_sectors=atoi(argv[3]);
		geometry_iso_sectorInterleave=atoi(argv[2]);
		floppy_iso_buildSectorInterleavingLut();
		floppy_iso_evaluateSectorInterleaving();
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

		readDisk(f,0,geometry_cylinders-1);

		fclose(f);
		printf("Image erfolgreich geschrieben !\n");
	}
	else if (!strcmp(argv[1],"write") && argc >= 3)
	{

		if (argc==4)
		{
			filename=argv[3];
			parseFormatString(argv[2]);
			calcGeometry(filename);
		}
		else
		{
			filename=argv[2];
		}

		configureController(CALIBRATE_MAGIC);

		f=fopen(filename,"r");
		assert(f);

		writeDisk(f,0,geometry_cylinders-1);

		fclose(f);
		printf("Image erfolgreich gelesen !\n");
	}
	else if (!strcmp(argv[1],"discover") && argc == 2)
	{
		discoverFormat();
	}
	


	return 0;
}


