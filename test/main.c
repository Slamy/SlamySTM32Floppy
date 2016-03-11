#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_sector.h"
#include "floppy_mfm.h"
#include "floppy_settings.h"
#include "floppy_control.h"

struct TIM4_stub TIM4Stub;

struct TIM4_stub* TIM4=&TIM4Stub;
struct TIM4_stub* TIM2=&TIM4Stub;
struct TIM4_stub* TIM3=&TIM4Stub;

volatile unsigned int indexHappened=0;
void floppy_selectDensity(enum Density val)
{

}

void printEvenLongBin(unsigned long val)
{

	int i;

	for (i=0;i<16;i++)
	{
		if (val&0x40000000)
		{
			printf("-");
		}
		else
		{
			printf("_");
		}

		val<<=2;
	}
}

void printLongBin(unsigned long val)
{

	int i;

	for (i=0;i<32;i++)
	{
		if (val&0x80000000)
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

uint32_t TIM_GetCapture3_ret;

uint32_t TIM_GetCapture3()
{
	return TIM_GetCapture3_ret;
}

void TIM_ClearITPendingBit(struct TIM4_stub *a, int b)
{

}

#define TRANSITION_MAXANZ 80000
uint32_t currentTime=0;
uint32_t transitionTimes[TRANSITION_MAXANZ];
uint32_t transitionTimes_anz=0;

uint32_t addTransitionTimeDisabled=0;
uint32_t enableTransitionTimeRead=0;

void addTransitionTime(unsigned int diff)
{
	if (addTransitionTimeDisabled)
		return;

	assert(transitionTimes_anz<TRANSITION_MAXANZ);
	//printf("Trans:%d\n",diff);
	currentTime+=diff;
	transitionTimes[transitionTimes_anz]=currentTime;
	transitionTimes_anz++;

}


int transTimeAccu=0;

void addMfmRawTransitionTimes_char(unsigned char mfmRaw)
{
	//printf("addMfmRawTransitionTimes %x\n",mfmRaw);

	int i;

	for (i=0;i<8;i++)
	{
		if (mfmRaw & 0x80)
		{
			transTimeAccu+=MFM_BITTIME_DD>>1;
			addTransitionTime(transTimeAccu);
			transTimeAccu=0;
		}
		else
		{
			transTimeAccu+=MFM_BITTIME_DD>>1;
		}

		mfmRaw<<=1;

	}
}


void addMfmRawTransitionTimes_short(unsigned short mfmRaw)
{
	//printf("addMfmRawTransitionTimes %x\n",mfmRaw);

	int i;

	for (i=0;i<16;i++)
	{
		if (mfmRaw&0x8000)
		{
			transTimeAccu+=MFM_BITTIME_DD>>1;
			addTransitionTime(transTimeAccu);
			transTimeAccu=0;
		}
		else
		{
			transTimeAccu+=MFM_BITTIME_DD>>1;
		}

		mfmRaw<<=1;

	}
}

void mfm_amiga_encode_even(unsigned long data)
{
	//printf("mfm_amiga_encode_even %x\n",data);

	int i;

	for (i=0;i<16;i++)
	{
		if (data&0x40000000) //das oberwertigste even bit im long word wird herangezogen
		{
			//Eine 1 ist immer 01. 0 ist eine Pause. 1 ist eine Pause mit Transition.
			transTimeAccu+=MFM_BITTIME_DD;
			addTransitionTime(transTimeAccu);
			transTimeAccu=0;
			mfm_lastBit=1;
		}
		else
		{
			if (mfm_lastBit)
			{
				//Wenn das letzte Bit eine 1 war, dann brauchen wir hier nichts zu tun. 00. Also 2 Pausen

				transTimeAccu+=MFM_BITTIME_DD;
			}
			else
			{
				//Das letzte Bit war schon eine 0. Dann direkt eine Transition NACH einer Pause erzeugen und eine Pause hinten dran.
				transTimeAccu+=MFM_BITTIME_DD>>1;
				addTransitionTime(transTimeAccu);
				transTimeAccu=MFM_BITTIME_DD>>1;
			}

			mfm_lastBit=0;
		}

		data<<=2;
	}
}

void mfm_amiga_encode_odd(unsigned long data)
{
	mfm_amiga_encode_even(data>>1);
}



uint32_t mfm_lastBit=0;

unsigned short mfm_iso_encode(unsigned char data)
{
	int i;
	unsigned short rawData=0;


	for (i=0;i<8;i++)
	{
		if (data&0x80)
		{
			rawData=(rawData<<2)|1;
			mfm_lastBit=1;
		}
		else
		{
			if (mfm_lastBit)
				rawData=(rawData<<2);
			else
				rawData=(rawData<<2)|2;

			mfm_lastBit=0;
		}

		data=data<<1;

	}

	return rawData;
}


void STM_EVAL_LEDInit(Led_TypeDef Led)
{

}

void STM_EVAL_LEDOff(Led_TypeDef Led)
{

}

void STM_EVAL_LEDOn(Led_TypeDef Led)
{

}

void TIM_ITConfig(TIM_TypeDef* TIMx, unsigned short TIM_IT, FunctionalState NewState)
{

}

void TIM_SetCompare3(TIM_TypeDef* TIMx, uint32_t Compare3)
{

}

void TIM_ForcedOC3Config(TIM_TypeDef* TIMx, uint16_t TIM_ForcedAction)
{

}

void floppy_setMotor(int drive, int val)
{

}

void floppy_setHead(int head)
{

}

void setupStepTimer(int waitTime)
{

}

void floppy_stepToCylinder00()
{

}

void floppy_stepToCylinder(unsigned int wantedCyl)
{

}

int floppy_waitForIndex()
{
	return 0;
}

void floppy_setWriteGate(int val)
{

}



FlagStatus TIM_GetFlagStatus(TIM_TypeDef* TIMx, unsigned short TIM_FLAG)
{
	return RESET;
}

extern unsigned int sectorsRead;
void activeWaitCbk()
{
	static int waitCycles=0;
	static int transTimeI=0;
	//printf("activeWaitCbk %d %d %d %d\n",waitCycles,transTimeI,transitionTimes_anz,enableTransitionTimeRead);
	waitCycles++;
	if ((waitCycles % 5) ==0)
	{
		if (transTimeI < transitionTimes_anz && enableTransitionTimeRead)
		{
			TIM_GetCapture3_ret=transitionTimes[transTimeI];
			//printf("Trans: %d\n",TIM_GetCapture3_ret);
			TIM2_IRQHandler();
			transTimeI++;
		}

		TIM4_IRQHandler();



	}
}

void main()
{
	int i;
	printf("Slamy STM32 Floppy Controller - C Unit\n");


	//make a sector header for ISO
#if 0
	crc=0xFFFF;
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);
	crc_shiftByte(0xfe);
	crc_shiftByte(0x43); //cylinder
	crc_shiftByte(0x1); //header
	crc_shiftByte(0x3); //sector
	crc_shiftByte(0x2); //Für 512 Sektoren


	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));

	addMfmRawTransitionTimes_short(0x4489);
	addMfmRawTransitionTimes_short(0x4489);
	addMfmRawTransitionTimes_short(0x4489);

	addMfmRawTransitionTimes_short(mfm_iso_encode(0xfe));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x43));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x1));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x3));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x2));
	addMfmRawTransitionTimes_short(mfm_iso_encode(crc>>8));
	addMfmRawTransitionTimes_short(mfm_iso_encode(crc&0xFF));

	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));

	crc=0xFFFF;
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);
	crc_shiftByte(0xa1);
	crc_shiftByte(0xfb);

	addMfmRawTransitionTimes_short(0x4489);
	addMfmRawTransitionTimes_short(0x4489);
	addMfmRawTransitionTimes_short(0x4489);

	addMfmRawTransitionTimes_short(mfm_iso_encode(0xfb));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x42));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x42));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x42));

#if 0
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



	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));

	addMfmRawTransitionTimes_short(0x4489);
	addMfmRawTransitionTimes_short(0x4489);
	addMfmRawTransitionTimes_short(0x4489);

	addMfmRawTransitionTimes_short(mfm_iso_encode(0xfe));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x42));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x1));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x23));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x2));
	addMfmRawTransitionTimes_short(mfm_iso_encode(crc>>8));
	addMfmRawTransitionTimes_short(mfm_iso_encode(crc&0xFF));

	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
#endif
	crc=0xFFFF;
	floppy_configureFormat(FLOPPY_FORMAT_ISO_DD,0,0,0);

	floppy_readTrackMachine_init();
	for(;;)
	{
		//Ein bissle die Machine ausführen....

		floppy_iso_trackDataMachine(0x43,1);

	}
#endif


#if 0
	//Amiga Sector Header - Und zwar alle 11

	int i,j;
	mfm_lastBit=0;
	transTimeAccu=0;

	for (i=0;i<12;i++)
	{
		unsigned int sector=i;
		unsigned int remSec=12-sector;

		uint32_t secHead=0xff000000 | (sector<<8) | remSec;
		//Amiga Sector Header

		addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
		addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));

		addMfmRawTransitionTimes_short(0x4489);
		addMfmRawTransitionTimes_short(0x4489);

		//Longword Block Sector Header
		mfm_amiga_encode_odd(secHead);
		mfm_amiga_encode_even(secHead);

		//16 Byte Block OS Info.... immer 0...
		for (j=0; j < 16/4; j++)
			mfm_amiga_encode_odd(0);
		for (j=0; j < 16/4; j++)
			mfm_amiga_encode_even(0);

		//Longword Header Checksumme
		mfm_amiga_encode_odd(secHead);
		mfm_amiga_encode_even(secHead);

		//Longword Daten Checksumme
		mfm_amiga_encode_odd(0);
		mfm_amiga_encode_even(0);

		//512 Byte Daten
		for (j=0; j< 512/4; j++)
			mfm_amiga_encode_odd(j);

		for (j=0; j< 512/4; j++)
			mfm_amiga_encode_even(j);


	}

	printf("Finished creating Transitions\n");

	floppy_configureFormat(FLOPPY_FORMAT_AMIGA_DD,0,0,0);
	addTransitionTimeDisabled=1;
	enableTransitionTimeRead=1;

	floppy_readTrackMachine_init();
	while (sectorsRead < 11)
	{
		//Ein bissle die Machine ausführen....

		floppy_amiga_readTrackMachine(0,0);
	}

	for (i=0;i<512/4;i++)
	{
		printf("%x\n",trackBuffer[i]);
	}
#endif


#if 0
	//ISO MFM Write Basistest
	printf("Soll:\n");

	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));
	addMfmRawTransitionTimes_short(0x4489);
	addMfmRawTransitionTimes_short(0x4489);
	addMfmRawTransitionTimes_short(mfm_iso_encode(0x00));

	printf("Ist:\n");

	mfm_configureWrite(MFM_ENCODE,8);
	mfm_blockedWrite(0);

	mfm_configureWrite(MFM_RAW,16);
	mfm_blockedWrite(0x4489);
	mfm_blockedWrite(0x4489);

	mfm_configureWrite(MFM_ENCODE,8);
	mfm_blockedWrite(0);
	mfm_blockedWrite(0);
	mfm_blockedWrite(0);

#endif

#if 0
	floppy_configureFormat(FLOPPY_FORMAT_ISO_DD,0,0,0);

	floppy_iso_writeTrack(0,0,0);
	addTransitionTimeDisabled=1;
	enableTransitionTimeRead=1;
	printf("Write finished!\n");


	floppy_readTrackMachine_init();
	for(;;)
	{
		//Ein bissle die Machine ausführen....

		floppy_iso_readTrackMachine(0,0);
	}

#endif

#if 0
	floppy_configureFormat(FLOPPY_FORMAT_AMIGA_DD,0,0,0);

	floppy_amiga_writeTrack(0,0,0);
	addTransitionTimeDisabled=1;
	enableTransitionTimeRead=1;
	printf("Write finished!\n");


	floppy_readTrackMachine_init();
	for(;;)
	{
		//Ein bissle die Machine ausführen....

		floppy_amiga_readTrackMachine(0,0);
	}
#endif

#if 0
	uint8_t *trackData=trackBuffer;

	trackData[0]=0;
	trackData[1]=255;
	for (i=2;i<255;i++)
	{
		trackData[i]=0xaa;
	}

	floppy_configureFormat(FLOPPY_FORMAT_RAW,0,0,0);
	floppy_writeRawTrack(0,0);

#endif

	floppy_configureFormat(FLOPPY_FORMAT_AMIGA_DD,0,0,0);
	addMfmRawTransitionTimes_char(0xaa);
	addMfmRawTransitionTimes_char(0xaa);
	addMfmRawTransitionTimes_char(0xaa);
	addMfmRawTransitionTimes_char(0xaa);
	addMfmRawTransitionTimes_char(0xa9);
	addMfmRawTransitionTimes_char(0x12);
	addMfmRawTransitionTimes_char(0x25);
	addMfmRawTransitionTimes_char(0x12);
	addMfmRawTransitionTimes_char(0x25);
	addMfmRawTransitionTimes_char(0x54);
	addMfmRawTransitionTimes_char(0xaa);
	addMfmRawTransitionTimes_char(0xaa);
	addMfmRawTransitionTimes_char(0x95);
	addMfmRawTransitionTimes_char(0x54);
	addMfmRawTransitionTimes_char(0xaa);
	addMfmRawTransitionTimes_char(0xaa);
	addMfmRawTransitionTimes_char(0xa4);
	addMfmRawTransitionTimes_char(0xaa);
	addMfmRawTransitionTimes_char(0xaa);
	addTransitionTimeDisabled=1;
	enableTransitionTimeRead=1;

	floppy_readTrackMachine_init();
	for(;;)
	{
		//Ein bissle die Machine ausführen....

		floppy_amiga_readTrackMachine(0,0);
	}

}

