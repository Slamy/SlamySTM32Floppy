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
unsigned char geometry_iso_sectorPos[MAX_SECTORS_PER_TRACK];

uint32_t geometry_iso_trackstart_4e=50;
uint32_t geometry_iso_trackstart_00=12;

uint32_t geometry_iso_before_idam_4e=50;
uint32_t geometry_iso_before_idam_00=12;

uint32_t geometry_iso_before_data_4e=22;
uint32_t geometry_iso_before_data_00=12;


void floppy_configureFormat(enum floppyFormat fmt, int cylinders, int heads, int sectors)
{
	int i;

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

	geometry_iso_trackstart_4e=50;
	geometry_iso_trackstart_00=12;

	geometry_iso_before_idam_4e=50;
	geometry_iso_before_idam_00=12;

	geometry_iso_before_data_4e=22;
	geometry_iso_before_data_00=12;

	for (i=0;i<MAX_SECTORS_PER_TRACK;i++)
	{
		geometry_iso_sectorPos[i]=i+1;
	}

	printf("Configured geometry: %d %d %d\n",
			(int)geometry_cylinders,
			(int)geometry_heads,
			(int)geometry_sectors);
}

