#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_mfm.h"


volatile unsigned long mfm_savedRawWord=0;
volatile unsigned char mfm_decodedByte=0;
volatile unsigned int mfm_inSync=0;
volatile unsigned int mfm_decodedByteValid=0;


extern void printShortBin(unsigned short val);
void printCharBin(unsigned char val);

static unsigned long rawMFM = 0;
static unsigned char decodedMFM = 0;
static unsigned int shiftedBits=0;

static unsigned int mfm_cellLength=MFM_BITTIME_DD/2;
static unsigned int mfm_transitionHandler=0;


//#define SYNC_WORD 0x5224
#define SYNC_WORD_ISO 0x4489 //broken A1
#define SYNC_WORD_AMIGA 0x44894489 //broken A1 *2

unsigned int diff;

void mfm_iso_decode()
{
	//printf("%4x %2d ",rawMFM,shiftedBits);
	//printShortBin(rawMFM);
	//printf(" ");
	//printCharBin(decodedMFM);
	//printf("\n");

	if (mfm_inSync && shiftedBits==16)
	{
		shiftedBits=0;
		//printf("Decoded:%2x\n",decodedMFM);
		mfm_decodedByte=decodedMFM;
		mfm_savedRawWord=rawMFM;
		mfm_decodedByteValid=1;
	}

	if ((rawMFM & 0xffff)==SYNC_WORD_ISO) //IAM sync word is broken A1.
	{
		shiftedBits=0;
		rawMFM=0;
		decodedMFM=0;
		mfm_inSync++;
		if (mfm_inSync>5)
			mfm_inSync=5;
	}
}

void mfm_iso_transitionHandler()
{
	if (diff > MAXIMUM_VALUE)
	{
		//Die letzte Transition ist zu weit weg. Desynchronisiert...
		mfm_inSync=0;
		//printf("UNSYNC\n");
	}
	else
	{
		//Die leeren Zellen werden nun abgezogen und 0en werden eingeshiftet.
		//printf("diff:%d\n",diff);
		while (diff > mfm_cellLength + mfm_cellLength/2) //+mfm_cellLength/2 ist die Toleranz die genau auf die Mitte gesetzt wird.
		{
			diff-=mfm_cellLength;
			rawMFM<<=1;
			if ((shiftedBits&1)!=0)
			{
				decodedMFM<<=1;
			}
			shiftedBits++;
			mfm_iso_decode();
		}

		//Es bleibt eine 1 übrig.
		rawMFM<<=1;
		rawMFM|=1;
		if ((shiftedBits&1)!=0)
		{
			//Die geraden Bits bestimmen die tatsächlichen Daten.
			decodedMFM<<=1;
			decodedMFM|=1;
		}
		shiftedBits++;
		mfm_iso_decode();
	}
}



void mfm_amiga_decode()
{
	//printf("%4x %2d ",rawMFM,shiftedBits);
	//printShortBin(rawMFM);
	//printf(" ");
	//printCharBin(decodedMFM);
	//printf("\n");

	if (mfm_inSync && shiftedBits==32)
	{
		shiftedBits=0;
		//printf("Decoded:%2x\n",decodedMFM);
		mfm_decodedByte=decodedMFM;
		mfm_savedRawWord=rawMFM;
		mfm_decodedByteValid=1;
	}

	if (rawMFM==SYNC_WORD_AMIGA) //IAM sync word is broken A1.
	{
		shiftedBits=0;
		rawMFM=0;
		mfm_inSync++;
		if (mfm_inSync>5)
			mfm_inSync=5;
	}
}

void mfm_amiga_transitionHandler()
{
	if (diff > MAXIMUM_VALUE)
	{
		//Die letzte Transition ist zu weit weg. Desynchronisiert...
		mfm_inSync=0;
		//printf("UNSYNC\n");
	}
	else
	{
		//Die leeren Zellen werden nun abgezogen und 0en werden eingeshiftet.
		//printf("diff:%d\n",diff);
		while (diff > mfm_cellLength + mfm_cellLength/2) //+mfm_cellLength/2 ist die Toleranz die genau auf die Mitte gesetzt wird.
		{
			diff-=mfm_cellLength;
			rawMFM<<=1;
			shiftedBits++;
			mfm_amiga_decode();
		}

		//Es bleibt eine 1 übrig.
		rawMFM<<=1;
		rawMFM|=1;
		shiftedBits++;
		mfm_amiga_decode();
	}
}

/*
volatile unsigned int diffCollector[1000];
volatile unsigned int diffCollector_Anz=0;

void activeWaitMeasure(void)
{
	unsigned int lastTransitionTime=0;

	while (diffCollector_Anz<999)
	{
		unsigned int transitionTime;

		do
		{
			transitionTime=TIM_GetCapture3(TIM2);
		} while (transitionTime==lastTransitionTime);

		unsigned int diff=transitionTime - lastTransitionTime;

		diffCollector[diffCollector_Anz]=diff;
		diffCollector_Anz++;

		lastTransitionTime=transitionTime;
	}

}
*/

void TIM2_IRQHandler(void)
{
	static unsigned int lastTransitionTime=0;

	unsigned int intStart=TIM_GetCounter(TIM2);

	unsigned int transitionTime=TIM_GetCapture3(TIM2);
	diff=transitionTime - lastTransitionTime;
	TIM_ClearITPendingBit(TIM2, TIM_IT_CC3);

	mfm_iso_transitionHandler();

	//printf("%u\n",diff);
	/*
	diffCollector[diffCollector_Anz]=diff;
	if (diffCollector_Anz<999)
		diffCollector_Anz++;
	*/



	lastTransitionTime=transitionTime;


}


unsigned short mfm_iso_encode(unsigned char data)
{
	int i;
	unsigned short rawData=0;
	static int lastBit=0;

	for (i=0;i<8;i++)
	{
		if (data&0x80)
		{
			rawData=(rawData<<2)|1;
			lastBit=1;
		}
		else
		{
			if (lastBit)
				rawData=(rawData<<2);
			else
				rawData=(rawData<<2)|2;

			lastBit=0;
		}

		data=data<<1;

	}

	return rawData;
}


void mfm_init()
{
/*
	   ===================================================================
			  TIM Driver: how to use it in Input Capture Mode
	   ===================================================================
	   To use the Timer in Input Capture mode, the following steps are mandatory:

	   1. Enable TIM clock using RCC_APBxPeriphClockCmd(RCC_APBxPeriph_TIMx, ENABLE) function
*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
/*
	   2. Configure the TIM pins by configuring the corresponding GPIO pins
*/
	  GPIO_InitTypeDef  GPIO_InitStructure;

	  /* Configure the GPIO_LED pin */
	  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	  GPIO_Init(GPIOA, &GPIO_InitStructure);

	  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_TIM2);
/*
	   2. Configure the Time base unit as described in the first part of this driver,
		  if needed, else the Timer will run with the default configuration:
		  - Autoreload value = 0xFFFF
		  - Prescaler value = 0x0000
		  - Counter mode = Up counting
		  - Clock Division = TIM_CKD_DIV1
*/
	TIM_TimeBaseInitTypeDef timInit;
	timInit.TIM_Prescaler=0;
	timInit.TIM_CounterMode=TIM_CounterMode_Up;
	timInit.TIM_Period=0xFFFFFFFF;
	timInit.TIM_ClockDivision=TIM_CKD_DIV1;
	timInit.TIM_RepetitionCounter=0x50;
	TIM_TimeBaseInit(TIM2, &timInit);
/*
	   3. Fill the TIM_ICInitStruct with the desired parameters including:
		  - TIM Channel: TIM_Channel
		  - TIM Input Capture polarity: TIM_ICPolarity
		  - TIM Input Capture selection: TIM_ICSelection
		  - TIM Input Capture Prescaler: TIM_ICPrescaler
		  - TIM Input CApture filter value: TIM_ICFilter

	   4. Call TIM_ICInit(TIMx, &TIM_ICInitStruct) to configure the desired channel with the
		  corresponding configuration and to measure only frequency or duty cycle of the input signal,
		  or,
		  Call TIM_PWMIConfig(TIMx, &TIM_ICInitStruct) to configure the desired channels with the
		  corresponding configuration and to measure the frequency and the duty cycle of the input signal
*/

	TIM_ICInitTypeDef icInit;

	icInit.TIM_Channel=TIM_Channel_3;
	icInit.TIM_ICPolarity=TIM_ICPolarity_Rising;
	icInit.TIM_ICSelection=TIM_ICSelection_DirectTI;
	icInit.TIM_ICPrescaler=TIM_ICPSC_DIV1;
	icInit.TIM_ICFilter=0;

	TIM_ICInit(TIM2,&icInit);

/*
	   5. Enable the NVIC or the DMA to read the measured frequency.
*/
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
/*
	   6. Enable the corresponding interrupt (or DMA request) to read the Captured value,
		  using the function TIM_ITConfig(TIMx, TIM_IT_CCx) (or TIM_DMA_Cmd(TIMx, TIM_DMA_CCx))
*/
	TIM_ITConfig(TIM2,TIM_IT_CC3,ENABLE);
/*
	   7. Call the TIM_Cmd(ENABLE) function to enable the TIM counter.
*/
	TIM_Cmd(TIM2,ENABLE);
/*
	   8. Use TIM_GetCapturex(TIMx); to read the captured value.

	   Note1: All other functions can be used separately to modify, if needed,
			  a specific feature of the Timer.
 */

}

void mfm_setDecodingMode(enum mfmMode mode)
{
	switch (mode)
	{
		case MFM_ISO_DD:
			mfm_cellLength=MFM_BITTIME_DD>>1;
			mfm_transitionHandler=0;

			break;

		case MFM_ISO_HD:
			mfm_cellLength=MFM_BITTIME_HD>>1;
			mfm_transitionHandler=0;

			break;

		case MFM_AMIGA_DD:
			mfm_cellLength=MFM_BITTIME_DD>>1;
			mfm_transitionHandler=1;
			break;
	}


}

void mfm_setEnableState(FunctionalState state)
{
	TIM_ITConfig(TIM2,TIM_IT_CC3,state);
}
