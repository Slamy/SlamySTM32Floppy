#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_sector.h"



void printShortBin(unsigned short val)
{

	int i;

	for (i=0;i<16;i++)
	{
		if (val&0x8000)
		{
			printf("-");
		}
		else
		{
			printf("_");
		}

		val<<=1;
	}
}



void printCharBin(unsigned char val)
{

	int i;

	for (i=0;i<8;i++)
	{
		if (val&0x80)
		{
			printf("-");
		}
		else
		{
			printf("_");
		}

		val<<=1;
	}
}


//stubs

unsigned int TIM_GetCapture3_ret;

unsigned int TIM_GetCapture3()
{
	return TIM_GetCapture3_ret;
}

void TIM_ClearITPendingBit(int a, int b)
{

}

unsigned int currentTime=0;
unsigned int transitionTimes[2000];
unsigned int transitionTimes_anz=0;

void addTransitionTime(unsigned short diff)
{
	//printf("Trans:%f\n",(float)diff/(float)LENGTH_MFM_CELL);
	currentTime+=diff;
	transitionTimes[transitionTimes_anz]=currentTime;
	transitionTimes_anz++;
}

void addMfmRawTransitionTimes(unsigned short mfmRaw)
{
	//printf("addMfmRawTransitionTimes %x\n",mfmRaw);
	static unsigned short accumulatedTime=0;

	int i;

	for (i=0;i<16;i++)
	{
		if (mfmRaw&0x8000)
		{
			accumulatedTime+=LENGTH_MFM_CELL;
			addTransitionTime(accumulatedTime);
			accumulatedTime=0;
		}
		else
		{
			accumulatedTime+=LENGTH_MFM_CELL;
		}

		mfmRaw<<=1;

	}
}


struct pt floppy_sectorRead_thread_pt;

void main()
{
	printf("Slamy STM32 Floppy Controller - C Unit\n");

	PT_INIT(&floppy_sectorRead_thread_pt);

	//make a sector header

	crc=0xFFFF;
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);
	crc_shiftByte(0xfe);
	crc_shiftByte(0x43); //cylinder
	crc_shiftByte(0x1); //header
	crc_shiftByte(0x23); //sector
	crc_shiftByte(0x2); //Für 512 Sektoren



	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));

	addMfmRawTransitionTimes(0x4489);
	addMfmRawTransitionTimes(0x4489);
	addMfmRawTransitionTimes(0x4489);

	addMfmRawTransitionTimes(mfm_encode(0xfe));
	addMfmRawTransitionTimes(mfm_encode(0x43));
	addMfmRawTransitionTimes(mfm_encode(0x1));
	addMfmRawTransitionTimes(mfm_encode(0x23));
	addMfmRawTransitionTimes(mfm_encode(0x2));
	addMfmRawTransitionTimes(mfm_encode(crc>>8));
	addMfmRawTransitionTimes(mfm_encode(crc&0xFF));

	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));


	//und nochmal. aber mit falscher crc

	crc=0xFFFF;
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);
	crc_shiftByte(0xfe);
	crc_shiftByte(0x43); //cylinder
	crc_shiftByte(0x1); //header
	crc_shiftByte(0x23); //sector
	crc_shiftByte(0x2); //Für 512 Sektoren



	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));

	addMfmRawTransitionTimes(0x4489);
	addMfmRawTransitionTimes(0x4489);
	addMfmRawTransitionTimes(0x4489);

	addMfmRawTransitionTimes(mfm_encode(0xfe));
	addMfmRawTransitionTimes(mfm_encode(0x42));
	addMfmRawTransitionTimes(mfm_encode(0x1));
	addMfmRawTransitionTimes(mfm_encode(0x23));
	addMfmRawTransitionTimes(mfm_encode(0x2));
	addMfmRawTransitionTimes(mfm_encode(crc>>8));
	addMfmRawTransitionTimes(mfm_encode(crc&0xFF));

	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));
	addMfmRawTransitionTimes(mfm_encode(0x00));


	for (int i=0; i < transitionTimes_anz;i++)
	{
		TIM_GetCapture3_ret=transitionTimes[i];
		TIM2_IRQHandler();

		//Ein bissle den Thread ausführen....
		for (int j=0; j < 6;j++)
		{
			PT_SCHEDULE(floppy_sectorRead_thread(&floppy_sectorRead_thread_pt));
		}
	}

}

