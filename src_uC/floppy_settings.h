
#ifndef FLOPPY_SETTINGS_H
#define FLOPPY_SETTINGS_H

#include <stdint.h>
#include "floppy_mfm.h"

enum floppyFormat
{
	FLOPPY_FORMAT_UNKNOWN,
	FLOPPY_FORMAT_ISO_DD,
	FLOPPY_FORMAT_ISO_HD,
	FLOPPY_FORMAT_AMIGA_DD
};

extern uint32_t geometry_payloadBytesPerSector;
extern uint32_t geometry_cylinders;
extern uint32_t geometry_heads;
extern uint32_t geometry_sectors;
extern uint32_t geometry_iso_sectorInterleave;

extern uint32_t geometry_iso_trackstart_4e;
extern uint32_t geometry_iso_trackstart_00;

extern uint32_t geometry_iso_before_idam_4e;
extern uint32_t geometry_iso_before_idam_00;

extern uint32_t geometry_iso_before_data_4e;
extern uint32_t geometry_iso_before_data_00;

void floppy_configureFormat(enum floppyFormat fmt, int cylinders, int heads, int sectors, int interleave);

#endif


