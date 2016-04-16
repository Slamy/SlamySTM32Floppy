
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

#define BULK_EP_OUT     0x81
#define BULK_EP_IN      0x01

struct termios oldkey, newkey;
int tty;

libusb_device_handle *devicehandle;
unsigned short crc=0xFFFF;

uint32_t geometry_cylinders=0;
uint32_t geometry_heads=0;
char geometry_sectorsPerCylinder[MAX_CYLINDERS];
unsigned char geometry_iso_gap3length[MAX_CYLINDERS];
unsigned char geometry_iso_fillerByte[MAX_CYLINDERS];
unsigned short geometry_actualSectorSize[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK]; //tatsächliche Größe der Daten in Byte
unsigned char geometry_iso_sectorId[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK]; //Interleaving ist damit auch abgedeckt
unsigned char geometry_iso_sectorHeaderSize[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK]; //z.B. 2 für 512 Byte Sektoren. nur untere 3 bits werden benutzt
unsigned char geometry_iso_sectorErased[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK];

#define CONFIGFLAG_ISO_NO_ROOM_REDUCE_GAP 1
#define CONFIGFLAG_ISO_NO_ROOM_REDUCE_BITRATE 2
#define CONFIGFLAG_INVERT_SIDES 4
#define CONFIGFLAG_FLIPPY_SIMULATE_INDEX 8

uint32_t configuration_flags=0;

int geometry_iso_sectorInterleave=0;

//#define CYLINDER_BUFFER_SIZE MAX_HEADS * MAX_SECTORS_PER_TRACK * MAX_SECTOR_SIZE //für HD ausreichend

unsigned char image_cylinderBuf[MAX_CYLINDERS][CYLINDER_BUFFER_SIZE];
int image_cylinderSize[MAX_CYLINDERS];

static const unsigned int isoSectorSizes[]={
		128, //0
		256, //1
		512, //2
		1024, //3
		2048, //4
		4096, //5
		8192 //6
};



enum floppyFormat format=FLOPPY_FORMAT_UNKNOWN;


char *formatStr[]=
{
	"UNKNOWN",
	"Iso DD",
	"Iso HD",
	"Amiga DD",
	"C64",
	"Raw MFM"
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


void measureRpm()
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
	sendBuf[6]=6; //Measure Rpm
	sendFrame(sendBuf,7);

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

void writeCylinderFromImage(int cylinder)
{
	int i;
	unsigned char cylinderBuf[CYLINDER_BUFFER_SIZE];
	int bytes_read;
	int total_bytes_read;
	unsigned char sendBuf[64];
	unsigned char recvBuf[64];
	int tracksize=0;


	if (image_cylinderSize[cylinder]==0)
	{
		printf("Empty cylinder...\n");
		return;
	}
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
			//printf("writeCylinderFromImage %d %d\n",i,geometry_iso_sectorErased[cylinder][i]);
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
		exit(1);
	}

	if (memcmp("WCR",recvBuf,3))
	{
		printf("Unexpected answer 2!\n");
		exit(1);
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



void configureController()
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
	sendBuf[10]=configuration_flags;

	sendFrame(sendBuf,11);

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
	printf("configureController %d %d %d finished!\n",format,geometry_cylinders,geometry_heads);
}




void polarizeCylinder(int cylinder)
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
	sendBuf[6]=5; //polarize cylinder
	sendBuf[7]=cylinder;

	sendFrame(sendBuf,8);

	bytes_read=receiveFrame(recvBuf);
	if (bytes_read!=4)
	{
		printf("Unexpected answer!\n");
		exit(1);
	}

	if (memcmp("POL",recvBuf,3))
	{
		printf("Unexpected answer 2!\n");
		exit(1);
	}

	if (recvBuf[3]!=0)
	{
		printf("polarizeCylinder failed!\n");
		exit(1);
	}

	printf("polarizeCylinder %d finished!\n",cylinder);
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
	int cyl;

	for (cyl=first; cyl <= last; cyl++)
	{
		if (image_cylinderSize[cyl]>0)
			writeCylinderFromImage(cyl);
		else if(image_cylinderSize[cyl]==-1)
			polarizeCylinder(cyl);
	}
}



int floppy_iso_getSectorPos(int cyl,unsigned char sectorId)
{
	int i;
	for (i=0;i< geometry_sectorsPerCylinder[cyl]; i++)
	{
		//printf("floppy_iso_getSectorPos %d %d\n",i,sectorId);
		if (geometry_iso_sectorId[cyl][i]==sectorId)
		{
			return i;
		}
	}

	return -1;
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
			format=FLOPPY_FORMAT_ISO_DD;
			configuration_flags=CONFIGFLAG_ISO_NO_ROOM_REDUCE_BITRATE;
			return readImage_amstradCpc(path);
		}
		else if (!strcmp(fileTypeStr,".st"))
		{
			printf("Atari ST - Binary Image\n");
			format=FLOPPY_FORMAT_ISO_DD;
			configuration_flags=CONFIGFLAG_ISO_NO_ROOM_REDUCE_GAP;
			return readImage_noHeader(path);
		}
		else if (!strcmp(fileTypeStr,".adf"))
		{
			printf("Amiga - Binary Image\n");
			format=FLOPPY_FORMAT_AMIGA_DD;
			return readImage_noHeader(path);
		}
		else if (!strcmp(fileTypeStr,".ipf"))
		{
			printf("Interchangable Preservation Format\n");
			format=FLOPPY_FORMAT_RAW_MFM;
			return readImage_ipf(path);
		}
		else if (!strcmp(fileTypeStr,".d64"))
		{
			printf("C64 D64 Disk Image\n");
			format=FLOPPY_FORMAT_C64;
			return readImage_d64(path);
		}
		else if (!strcmp(fileTypeStr,".g64"))
		{
			printf("C64 G64 Disk Image\n");
			format=FLOPPY_FORMAT_RAW_MFM;
			return readImage_g64(path);
		}
#if 0
		else if (!strcmp(fileTypeStr,".nib"))
		{
			printf("C64 NIB Disk Image\n");
			format=FLOPPY_FORMAT_RAW_MFM;
			return readImage_nib(path);
		}
#endif
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

	printf("floppy_iso_buildSectorInterleavingLut %d %d\n",geometry_iso_sectorInterleave,geometry_sectors);

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

void setAttributesForEveryCylinder(int geometry_sectors, int gap3len, unsigned char fillerByte)
{
	int i;
	for (i=1;i<geometry_cylinders;i++)
	{
		geometry_sectorsPerCylinder[i]=geometry_sectors;
		geometry_iso_gap3length[i]=gap3len;
		geometry_iso_fillerByte[i]=fillerByte;
	}
}



void parseFormatString(char *str)
{

	if (!strncmp(str,"amiga",5))
	{
		geometry_cylinders=80;
		geometry_heads=2;
		setAttributesForEveryCylinder(11,22,0x4E);
		format=FLOPPY_FORMAT_AMIGA_DD;
	}
	else if (!strncmp(str,"isodd",5))
	{
		geometry_cylinders=80;
		geometry_heads=2;
		setAttributesForEveryCylinder(9,22,0x4E);
		format=FLOPPY_FORMAT_ISO_DD;
	}
	else if (!strncmp(str,"isohd",5))
	{
		geometry_cylinders=80;
		geometry_heads=2;
		setAttributesForEveryCylinder(18,22,0x4E);
		format=FLOPPY_FORMAT_ISO_HD;
	}
	else if (!strncmp(str,"flippy",6))
	{
		configuration_flags=CONFIGFLAG_FLIPPY_SIMULATE_INDEX;
		printf("The heads will now be switched!\n");
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
			setAttributesForEveryCylinder(atoi(str),22,0x4E);
		}
	}
}


int floppy_iso_getSectorNum(int cyl,int sectorPos)
{
	//return ((sectorPos-1)*(geometry_iso_sectorInterleave+1) % geometry_sectors) +1;
	return geometry_iso_sectorId[cyl][sectorPos];
}

void floppy_iso_evaluateSectorInterleaving(int cyl, int geometry_sectors)
{
	int sectorpos=0;
	int expectedSector=1;

	for (sectorpos=0; sectorpos < geometry_sectors ; sectorpos++)
	{
		printf("%2d ",floppy_iso_getSectorNum(cyl,sectorpos));
	}
	printf("\n\n");

	while (expectedSector <= geometry_sectors)
	{
		for (sectorpos=0; sectorpos < geometry_sectors; sectorpos++)
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
		geometry_iso_sectorInterleave = atoi(argv[2]);
		floppy_iso_buildSectorInterleavingLut(geometry_sectors);
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
		configureController();

		writeDisk(0,geometry_cylinders-1);

		printf("Image erfolgreich auf Diskette geschrieben!\n");
	}
	else if (!strcmp(argv[1],"discover") && argc == 4)
	{
		discoverFormat(atoi(argv[2]),atoi(argv[3]));
	}
	else if (!strcmp(argv[1],"measure") && argc == 2)
	{
		measureRpm();
	}
	


	return 0;
}


