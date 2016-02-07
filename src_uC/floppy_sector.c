#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "pt.h"
#include "pt-sem.h"
#include "floppy_crc.h"
#include "floppy_mfm.h"
#include "floppy_sector.h"

#define BLOCKED_READ_MFM_OR_RESTART(pt,val) \
	PT_WAIT_WHILE(pt,mfm_decodingStatus == SYNC3); if (mfm_decodingStatus != DATA_VALID) {PT_RESTART(pt);}; mfm_decodingStatus=SYNC3; val=mfm_decodedByte;

#define WAIT_FOR_IDAM(pt) \
	PT_WAIT_UNTIL(pt,mfm_decodingStatus >= SYNC3);

unsigned char sectorData[512];

//arm-none-eabi-cpp floppy_sector.c -I CMSIS/ -IUtilities/ -I STM32F4xx_StdPeriph_Driver/inc -I pt-1.4/

PT_THREAD(floppy_sectorRead_thread(struct pt *pt))
{
	static unsigned int cyl=0;
	static unsigned int head=0;
	static unsigned int sec=0;
	static unsigned int temp=0;
	static unsigned int i=0;

	PT_BEGIN(pt);

	crc=0xFFFF; //reset crc
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);

	//printf("**** Wait for IDAM\n");
	mfm_decodingStatus = UNSYNC; //force unsync.
	WAIT_FOR_IDAM(pt);
	//printf("**** Got IDAM\n");


	BLOCKED_READ_MFM_OR_RESTART(pt,temp);
	if (temp!=0xa1)
	{
		//printf("**** temp!=0xa1 %x\n",temp);
		PT_RESTART(pt);
	}

	BLOCKED_READ_MFM_OR_RESTART(pt,temp);
	crc_shiftByte(temp);
	switch (temp) //IDAM - Sector Information
	{
	case 0xfe:
		BLOCKED_READ_MFM_OR_RESTART(pt,cyl);
		crc_shiftByte(cyl);

		BLOCKED_READ_MFM_OR_RESTART(pt,head);
		crc_shiftByte(head);

		BLOCKED_READ_MFM_OR_RESTART(pt,sec);
		crc_shiftByte(sec);

		BLOCKED_READ_MFM_OR_RESTART(pt,temp);
		if (temp!=2)
		{
			//printf("**** temp!=2 %x\n",temp);
			PT_RESTART(pt);
		}
		crc_shiftByte(temp);

		BLOCKED_READ_MFM_OR_RESTART(pt,temp);
		crc_shiftByte(temp);
		BLOCKED_READ_MFM_OR_RESTART(pt,temp);
		crc_shiftByte(temp);


		if (crc != 0) //crc has to be 0 at the end for a correct result
		{
			printf("**** idam crc error %d %d %d\n",cyl,head,sec);
			PT_RESTART(pt);
		}

		printf("SecHead: %d %d %d\n",cyl,head,sec);
		break;
	case 0xfb:
		for(i=0;i<512;i++)
		{
			BLOCKED_READ_MFM_OR_RESTART(pt,sectorData[i]);
			crc_shiftByte(sectorData[i]);
		}

		//Datenblock endet mit CRC
		BLOCKED_READ_MFM_OR_RESTART(pt,temp);
		crc_shiftByte(temp);
		BLOCKED_READ_MFM_OR_RESTART(pt,temp);
		crc_shiftByte(temp);

		if (crc != 0) //crc has to be 0 at the end for a correct result
		{
			printf("**** dam crc error %d %d %d\n",cyl,head,sec);
			PT_RESTART(pt);
		}

		printf("SecDat: %d %d %d\n",cyl,head,sec);
		break;
	default:
		PT_RESTART(pt);
	}



	PT_END(pt);
}



