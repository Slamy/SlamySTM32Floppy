/*
 * floppy_settings.c
 *
 *  Created on: 15.02.2016
 *      Author: andre
 */

#include <stdio.h>
#include "floppy_mfm.h"
#include "floppy_settings.h"
#include "floppy_control.h"
#include "floppy_sector.h"
#include "floppy_indexSim.h"
#include "floppy_flux.h"


enum floppyMedium preferedFloppyMedium=FLOPPY_MEDIUM_UNKNOWN;

uint32_t geometry_payloadBytesPerSector=512;
uint32_t geometry_cylinders=0;
uint32_t geometry_heads=0;
uint32_t geometry_sectors=0;

uint32_t configuration_flags=0;

enum floppyFormat geometry_format=FLOPPY_FORMAT_UNKNOWN;
unsigned short geometry_actualSectorSize[MAX_SECTORS_PER_TRACK]; //tatsächliche Größe der Daten in Byte

unsigned char geometry_iso_sectorId[MAX_SECTORS_PER_TRACK]; //Interleaving ist damit auch abgedeckt
unsigned char geometry_iso_sectorHeaderSize[MAX_SECTORS_PER_TRACK]; //z.B. 2 für 512 Byte Sektoren
unsigned char geometry_iso_sectorErased[MAX_SECTORS_PER_TRACK];

unsigned char geometry_iso_gap1_postIndex;	//32x 4E

unsigned char geometry_iso_gap2_preID_00;	//12x 00

unsigned char geometry_iso_gap3_postID;		//22x 4E
unsigned char geometry_iso_gap3_preData_00;	//12x 00

unsigned char geometry_iso_gap4_postData;	//24x 4E

unsigned char geometry_iso_gap5_preIndex;	//16x 4E

unsigned char geometry_iso_fillerByte;

unsigned char geometry_c64_gap_size;


void floppy_configureFormat(enum floppyFormat fmt, int cylinders, int heads, uint32_t flags)
{
	geometry_format=fmt;

	switch (fmt)
	{
		case FLOPPY_FORMAT_ISO_HD:
			flux_mode = FLUX_MODE_MFM_ISO;
			flux_decodeCellLength=MFM_BITTIME_HD>>1;

			geometry_cylinders=80;
			geometry_heads=2;
			geometry_sectors=18;
			geometry_payloadBytesPerSector=512;
			floppy_selectDensity(DENSITY_HIGH);
			preferedFloppyMedium=FLOPPY_MEDIUM_3_5_INCH;
			break;
		case FLOPPY_FORMAT_ISO_DD:
			flux_mode = FLUX_MODE_MFM_ISO;
			flux_decodeCellLength=MFM_BITTIME_DD>>1;

			geometry_cylinders=80;
			geometry_heads=2;
			geometry_sectors=9;
			geometry_payloadBytesPerSector=512;
			floppy_selectDensity(DENSITY_DOUBLE);
			preferedFloppyMedium=FLOPPY_MEDIUM_3_5_INCH;
			break;

		case FLOPPY_FORMAT_AMIGA_DD:
			flux_mode = FLUX_MODE_MFM_AMIGA;
			flux_decodeCellLength=MFM_BITTIME_DD>>1;

			geometry_cylinders=80;
			geometry_heads=2;
			geometry_sectors=11;
			geometry_payloadBytesPerSector=512;
			floppy_selectDensity(DENSITY_DOUBLE);
			preferedFloppyMedium=FLOPPY_MEDIUM_3_5_INCH;
			break;
		case FLOPPY_FORMAT_RAW_MFM:
			flux_mode = FLUX_MODE_MFM_ISO;
			flux_decodeCellLength=MFM_BITTIME_DD>>1;

			geometry_cylinders=80;
			geometry_heads=2;
			geometry_sectors=11;
			geometry_payloadBytesPerSector=512;
			floppy_selectDensity(DENSITY_DOUBLE);
			preferedFloppyMedium=FLOPPY_MEDIUM_3_5_INCH;
			break;

		case FLOPPY_FORMAT_C64:
			printf("geometry_format == FLOPPY_FORMAT_C64\n");
			flux_mode = FLUX_MODE_GCR_C64;
			flux_decodeCellLength=MFM_BITTIME_DD>>1;

			geometry_cylinders=40;
			geometry_heads=1;
			geometry_sectors=21;
			geometry_payloadBytesPerSector=256;

			floppy_c64_setTrackSettings(0);
			floppy_selectDensity(DENSITY_DOUBLE);
			preferedFloppyMedium=FLOPPY_MEDIUM_5_1_4_INCH;

			break;
		case FLOPPY_FORMAT_RAW_GCR:
			printf("geometry_format == FLOPPY_FORMAT_RAW_GCR\n");
			flux_mode = FLUX_MODE_GCR_C64;
			flux_decodeCellLength=MFM_BITTIME_DD>>1;

			geometry_cylinders=80;
			geometry_heads=2;
			geometry_sectors=1;
			geometry_payloadBytesPerSector=512;
			floppy_selectDensity(DENSITY_DOUBLE);
			preferedFloppyMedium=FLOPPY_MEDIUM_5_1_4_INCH;
			break;

		default:
			printf("floppy_configureFormat with wrong format:%d\n",fmt);
	}

	if (cylinders)
		geometry_cylinders=cylinders;

	if (heads)
		geometry_heads=heads;

	configuration_flags=flags;
	if (configuration_flags & CONFIGFLAG_FLIPPY_SIMULATE_INDEX)
		floppy_indexSim_setEnableState(ENABLE);
	else
		floppy_indexSim_setEnableState(DISABLE);


	geometry_iso_gap1_postIndex=32;	//32x 4E
	geometry_iso_gap2_preID_00=12;	//12x 00
	geometry_iso_gap3_postID=22;		//22x 4E
	geometry_iso_gap3_preData_00=12;	//12x 00
	geometry_iso_gap4_postData=24;	//24x 4E
	geometry_iso_gap5_preIndex=16;	//16x 4E

	geometry_iso_fillerByte=0x4E;

	printf("Configured geometry: %d %d %d\n",
			(int)geometry_cylinders,
			(int)geometry_heads,
			(int)geometry_sectors);
}



