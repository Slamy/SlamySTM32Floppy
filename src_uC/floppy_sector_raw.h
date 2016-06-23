
#ifndef FLOPPY_SECTOR_RAW_H
#define FLOPPY_SECTOR_RAW_H

extern uint8_t *verifySectorData;
extern int verifyablePartI;
extern int verifySectorDataBytesLeft;

void floppy_raw_find1541Sync();
uint8_t floppy_raw_getNextCylinderBufferByte();
void floppy_raw_findMFMSync();
void floppy_raw_getNextVerifyablePart();


extern int trackDataSize;
extern uint8_t *trackData;


#endif


