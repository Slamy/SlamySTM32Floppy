

extern uint32_t geometry_cylinders;
extern uint32_t geometry_heads;
extern enum floppyFormat format;

int readImage_ipf(const char *path);
int readImage_amstradCpc(const char *path);
int readImage_noHeader(const char *path);

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


#define MAX_SECTOR_SIZE 512
#define MAX_CYLINDERS 85
#define MAX_HEADS 2
#define MAX_SECTORS_PER_TRACK 18

#define CYLINDER_BUFFER_SIZE (14000 * 2) //basierend auf Turrican2.ipf

extern unsigned char image_cylinderBuf[MAX_CYLINDERS][CYLINDER_BUFFER_SIZE];
extern unsigned int image_cylinderSize[MAX_CYLINDERS];

void setAttributesForEveryCylinder(int geometry_sectors, int gap3len, unsigned char fillerByte);
void floppy_iso_buildSectorInterleavingLut();
void floppy_iso_evaluateSectorInterleaving(int cyl, int geometry_sectors);

extern unsigned char geometry_sectorsPerCylinder[MAX_CYLINDERS];
extern unsigned char geometry_iso_gap3length[MAX_CYLINDERS];
extern unsigned char geometry_iso_fillerByte[MAX_CYLINDERS];
extern unsigned short geometry_actualSectorSize[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK]; //tatsächliche Größe der Daten in Byte
extern unsigned char geometry_iso_sectorId[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK]; //Interleaving ist damit auch abgedeckt
extern unsigned char geometry_iso_sectorHeaderSize[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK]; //z.B. 2 für 512 Byte Sektoren. nur untere 3 bits werden benutzt
extern unsigned char geometry_iso_sectorErased[MAX_CYLINDERS][MAX_SECTORS_PER_TRACK];

int floppy_iso_getSectorPos(int cyl,unsigned char sectorId);

extern int geometry_iso_sectorInterleave;



