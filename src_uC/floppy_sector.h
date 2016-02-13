
#include "floppy_crc.h"
#include "floppy_mfm.h"

int floppy_readCylinder(unsigned int track, unsigned int expectedSectors);
void floppy_trackDataMachine_init();
enum mfmMode floppy_discoverFloppyFormat();

#define MAX_SECTORS_PER_CYLINDER (18*2)
#define MAX_SECTORS_PER_TRACK 18

#define SECTOR_SIZE 512
