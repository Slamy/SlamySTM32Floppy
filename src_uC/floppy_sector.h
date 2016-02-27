
#include <stdint.h>
#include "floppy_crc.h"
#include "floppy_mfm.h"

int floppy_writeAndVerifyCylinder(unsigned int cylinder);
int floppy_readCylinder(unsigned int track);
void floppy_readTrackMachine_init();
enum floppyFormat floppy_discoverFloppyFormat();

#define MAX_SECTORS_PER_CYLINDER (18*2)
#define MAX_SECTORS_PER_TRACK 18

#define MAX_SECTOR_SIZE 512


int floppy_iso_readTrackMachine(int expectedCyl, int expectedHead);
void floppy_iso_writeTrack(int cylinder, int head, int simulate);
void floppy_iso_calibrateTrackLength();
int floppy_iso_getSectorNum(int sectorPos);
void floppy_iso_evaluateSectorInterleaving();
void floppy_iso_buildSectorInterleavingLut();

int floppy_amiga_readTrackMachine(int expectedCyl, int expectedHead);


//extern uint8_t trackBuffer[SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER];
extern uint32_t trackBuffer[(MAX_SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER) / 4];

extern unsigned int trackReadState;
extern unsigned int sectorsRead;
extern unsigned char trackSectorRead[MAX_SECTORS_PER_CYLINDER];
extern unsigned char lastSectorDataFormat;
extern unsigned int verifyMode;
