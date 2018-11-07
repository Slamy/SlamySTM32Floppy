
#ifndef FLOPPY_SETTINGS_H
#define FLOPPY_SETTINGS_H

#include <stdint.h>
#include "floppy_mfm.h"

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


#define MAX_SECTORS_PER_TRACK 18
#define MAX_HEADS 2
#define MAX_SECTORS_PER_CYLINDER (MAX_SECTORS_PER_TRACK * MAX_HEADS)

#define MAX_SECTOR_SIZE 512

extern uint32_t geometry_payloadBytesPerSector;
extern uint32_t geometry_cylinders;
extern uint32_t geometry_heads;
extern uint32_t geometry_sectors;
extern enum floppyFormat geometry_format;
extern unsigned short geometry_actualSectorSize[MAX_SECTORS_PER_TRACK]; //tatsächliche Größe der Daten in Byte

extern unsigned char geometry_iso_sectorId[MAX_SECTORS_PER_TRACK]; //Interleaving ist damit auch abgedeckt
extern unsigned char geometry_iso_sectorHeaderSize[MAX_SECTORS_PER_TRACK]; //z.B. 2 für 512 Byte Sektoren
extern unsigned char geometry_iso_sectorErased[MAX_SECTORS_PER_TRACK];

extern unsigned char geometry_iso_fillerByte;

extern unsigned char geometry_iso_gap1_postIndex;	//32x 4E

extern unsigned char geometry_iso_gap2_preID_00;	//12x 00

extern unsigned char geometry_iso_gap3_postID;		//22x 4E
extern unsigned char geometry_iso_gap3_preData_00;	//12x 00

extern unsigned char geometry_iso_gap4_postData;	//24x 4E

extern unsigned char geometry_iso_gap5_preIndex;	//16x 4E


void floppy_configureFormat(enum floppyFormat fmt, int cylinders, int heads, int sectors);

#endif


