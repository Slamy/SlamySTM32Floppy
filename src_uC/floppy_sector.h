
#include <stdint.h>
#include "floppy_crc.h"
#include "floppy_mfm.h"
#include "floppy_settings.h"

int floppy_writeAndVerifyCylinder(unsigned int cylinder);
int floppy_readCylinder(unsigned int track);
void floppy_readTrackMachine_init();
enum floppyFormat floppy_discoverFloppyFormat();


int floppy_iso_readTrackMachine(int expectedCyl, int expectedHead);
int floppy_iso_writeTrack(int cylinder, int head, int simulate);
int floppy_iso_calibrateTrackLength();
int floppy_iso_getSectorNum(int sectorPos);
void floppy_iso_evaluateSectorInterleaving();
void floppy_iso_buildSectorInterleavingLut();

int floppy_amiga_readTrackMachine(int expectedCyl, int expectedHead);
int floppy_amiga_writeTrack(uint32_t cylinder, uint32_t head, int simulate);
int floppy_iso_reduceGap();

//extern uint8_t trackBuffer[SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER];
//extern uint32_t trackBuffer[(MAX_SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER) / 4];

#define CYLINDER_BUFFER_SIZE (14000 * 2) //basierend auf Turrican2.ipf
//#define CYLINDER_BUFFER_SIZE (20000 * 2) //testweise etwas mehr


extern uint32_t cylinderBuffer[CYLINDER_BUFFER_SIZE / 4];
extern uint32_t cylinderSize;

extern unsigned int trackReadState;
extern unsigned int sectorsRead;
extern unsigned char trackSectorRead[MAX_SECTORS_PER_CYLINDER];
extern unsigned int sectorsDetected;
extern unsigned char trackSectorDetected[MAX_SECTORS_PER_CYLINDER];

extern unsigned char lastSectorDataFormat;
extern unsigned int verifyMode;
