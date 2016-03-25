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


uint32_t geometry_payloadBytesPerSector=512;
uint32_t geometry_cylinders=0;
uint32_t geometry_heads=0;
uint32_t geometry_sectors=0;


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



void floppy_configureFormat(enum floppyFormat fmt, int cylinders, int heads, int sectors)
{
	geometry_format=fmt;

	switch (fmt)
	{
		case FLOPPY_FORMAT_ISO_HD:
			mfm_mode = MFM_MODE_ISO;
			mfm_decodeCellLength=MFM_BITTIME_HD>>1;

			geometry_cylinders=80;
			geometry_heads=2;
			geometry_sectors=18;
			floppy_selectDensity(DENSITY_HIGH);
			break;
		case FLOPPY_FORMAT_ISO_DD:
			mfm_mode = MFM_MODE_ISO;
			mfm_decodeCellLength=MFM_BITTIME_DD>>1;

			geometry_cylinders=80;
			geometry_heads=2;
			geometry_sectors=9;
			floppy_selectDensity(DENSITY_DOUBLE);
			break;

		case FLOPPY_FORMAT_AMIGA_DD:
			mfm_mode = MFM_MODE_AMIGA;
			mfm_decodeCellLength=MFM_BITTIME_DD>>1;

			geometry_cylinders=80;
			geometry_heads=2;
			geometry_sectors=11;
			floppy_selectDensity(DENSITY_DOUBLE);
			break;
		case FLOPPY_FORMAT_RAW:
			mfm_mode = MFM_MODE_ISO;
			mfm_decodeCellLength=MFM_BITTIME_DD>>1;

			geometry_cylinders=80;
			geometry_heads=2;
			geometry_sectors=11;
			floppy_selectDensity(DENSITY_DOUBLE);
			break;
		default:
			printf("floppy_configureFormat with wrong format:%d\n",fmt);
	}

	if (cylinders)
		geometry_cylinders=cylinders;

	if (heads)
		geometry_heads=heads;

	if (sectors)
		geometry_sectors=sectors;

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

