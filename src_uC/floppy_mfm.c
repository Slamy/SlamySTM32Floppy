#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_mfm.h"

volatile uint32_t mfm_savedRawWord=0;
volatile uint8_t mfm_decodedByte=0;
volatile uint32_t mfm_inSync=0;
volatile uint32_t mfm_decodedByteValid=0;

volatile static uint32_t rawMFM = 0;
volatile static uint8_t decodedMFM = 0;
volatile static uint32_t shiftedBits=0;

static unsigned int mfm_timeOut=0;
unsigned int mfm_errorHappened=0;

uint32_t mfm_cellLength=MFM_BITTIME_DD/2;

enum mfmMode mfm_mode;
uint32_t mfm_sectorsPerTrack=0;

//#define SYNC_WORD 0x5224
#define SYNC_WORD_ISO 0x4489 //broken A1
#define SYNC_WORD_AMIGA 0x44894489ul //broken A1 *2


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
		//STM_EVAL_LEDOn(LED3);
		shiftedBits=0;
		rawMFM=0;
		decodedMFM=0;

		mfm_inSync++;
		if (mfm_inSync>5)
			mfm_inSync=5;
		//STM_EVAL_LEDOff(LED3);
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
	/*
	printf("%08x %2d %d ",rawMFM,shiftedBits,mfm_inSync);
	printLongBin(rawMFM);
	printf("\n");
	*/

	if (mfm_inSync && shiftedBits==32)
	{
		shiftedBits=0;
		//printf("Decoded:%2x\n",decodedMFM);
		mfm_savedRawWord=rawMFM;
		mfm_decodedByteValid=1;
	}

	if (rawMFM==SYNC_WORD_AMIGA) //IAM sync word is broken A1 *2.
	//if ((rawMFM & 0xffff)==SYNC_WORD_ISO) //IAM sync word is broken A1.
	{
		//printf("Sync Word\n");
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

	//unsigned int intStart=TIM_GetCounter(TIM2);

	unsigned int transitionTime=TIM_GetCapture3(TIM2);
	diff=transitionTime - lastTransitionTime;
	TIM_ClearITPendingBit(TIM2, TIM_IT_CC3);

	if (mfm_mode==MFM_MODE_AMIGA)
	{
		mfm_amiga_transitionHandler();
	}
	else
	{
		mfm_iso_transitionHandler();
	}

	//printf("%u\n",diff);
	/*
	diffCollector[diffCollector_Anz]=diff;
	if (diffCollector_Anz<999)
		diffCollector_Anz++;
	*/

	lastTransitionTime=transitionTime;
}

static volatile uint32_t mfm_write_nextWord=0;
static volatile uint32_t mfm_write_nextWord_mask=0x80;
static volatile enum mfmEncodeMode mfm_write_nextWord_encodeMode=0;
static volatile uint32_t mfm_write_nextWord_len=0;

static volatile uint32_t mfm_write_busy=0;

static volatile uint32_t mfm_write_currentWord_mask=0x80;
static volatile uint32_t mfm_write_currentWord_len=0;
static volatile uint32_t mfm_write_currentWord_bit=0;
static volatile uint32_t mfm_write_currentWord=0;
static volatile enum mfmEncodeMode mfm_write_currentWord_encodeMode=0;

static volatile uint32_t mfm_write_lastBit=0;

uint16_t mfm_write_calcNextPauseLen(void)
{
	static uint16_t pauseLenAccu=0;
	uint16_t pauseLenRet=0;

	/*
	printf("mfm_write_calcNextPauseLen %x %d %d %x\n",mfm_write_currentWord,
											mfm_write_currentWord_encodeMode,
											mfm_write_currentWord_bit,
											mfm_write_currentWord_mask);
	*/

	while (!pauseLenRet) //wir akkumulieren Pausenzeiten, bis eine 1 Transition kommt.
	{

		if (mfm_write_currentWord_encodeMode == MFM_RAW)
		{
			if (mfm_write_currentWord & mfm_write_currentWord_mask)
			{
				//eine 1 bedeutet eine zelle pause und eine transition
				pauseLenRet=mfm_cellLength + pauseLenAccu;
				pauseLenAccu=0;
			}
			else
			{
				//eine 0 ist eine Pause. Wir müssen noch warten
				pauseLenAccu+=mfm_cellLength;
			}
		}
		else
		{
			if (mfm_write_currentWord & mfm_write_currentWord_mask)
			{
				//Eine 1 ist immer 01. 0 ist eine Pause. 1 ist eine Pause mit Transition.
				pauseLenRet=pauseLenAccu + (mfm_cellLength<<1);
				pauseLenAccu=0;
				mfm_write_lastBit=1;
			}
			else
			{
				if (mfm_write_lastBit)
				{
					//Wenn das letzte Bit eine 1 war, dann brauchen wir hier nichts zu tun. 00. Also 2 Pausen

					pauseLenAccu += mfm_cellLength<<1;
				}
				else
				{
					//Das letzte Bit war schon eine 0. Dann direkt eine Transition NACH einer Pause erzeugen und eine Pause hinten dran.
					pauseLenRet = mfm_cellLength+pauseLenAccu;
					pauseLenAccu = mfm_cellLength;

				}

				mfm_write_lastBit = 0;
			}
		}

		mfm_write_currentWord<<=1;
		if (mfm_write_currentWord_encodeMode == MFM_ENCODE_ODD)
			mfm_write_currentWord<<=1;

		mfm_write_currentWord_bit++;

		if (mfm_write_currentWord_bit >= mfm_write_currentWord_len)
		{
			mfm_write_currentWord_bit=0;
			mfm_write_currentWord=mfm_write_nextWord;
			mfm_write_currentWord_encodeMode=mfm_write_nextWord_encodeMode;
			mfm_write_currentWord_len=mfm_write_nextWord_len;
			mfm_write_currentWord_mask=mfm_write_nextWord_mask;
			mfm_write_busy=0;
		}
	}

	return pauseLenRet;
}

volatile static uint16_t lastCompare=0;

//volatile static uint16_t wasteTime=0;

void TIM4_IRQHandler(void)
{
	//printf("I\n");

	uint16_t pulseLen=mfm_write_calcNextPauseLen();

	//pulseLen*=4;

	TIM_ClearITPendingBit(TIM4, TIM_IT_CC3);
#ifdef CUNIT
	addTransitionTime(pulseLen);
#endif

	lastCompare+=pulseLen;
	//printf("mfmWrite %d %d\n",pulseLen,lastCompare);
	TIM_SetCompare3(TIM4,lastCompare);


	/*
	long int i;
	//printf("I\n");

	for (i=0;i<40000;i++)
	{
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
	}
	*/
	//printf("O\n");

	//TIM_ForcedOC3Config(TIM4,TIM_ForcedAction_InActive);

	/* Get the current value of the output compare mode */
	uint16_t tmpccmr2 = 0;
	tmpccmr2 = TIM4->CCMR2;

	/* Reset the OC1M Bits */
	tmpccmr2 &= (uint16_t)~TIM_CCMR2_OC3M;

	/* Configure The Forced output Mode */
	tmpccmr2 |= TIM_ForcedAction_InActive;

	/* Write to TIMx CCMR2 register */
	TIM4->CCMR2 = tmpccmr2;

	/* Reset the OC1M Bits */
	tmpccmr2 &= (uint16_t)~TIM_CCMR2_OC3M;

	/* Configure The Active output Mode */
	tmpccmr2 |= TIM_OCMode_Active;

	/* Write to TIMx CCMR2 register */
	TIM4->CCMR2 = tmpccmr2;


	/*
	for (i=0;i<3000*2000;i++)
	{
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
		wasteTime=42;
	}
	printf("A\n");
	*/

}



#ifndef CUNIT


void mfm_write_init()
{
	/*
	===================================================================
		  TIM Driver: how to use it in Output Compare Mode
	===================================================================
	To use the Timer in Output Compare mode, the following steps are mandatory:

	1. Enable TIM clock using RCC_APBxPeriphClockCmd(RCC_APBxPeriph_TIMx, ENABLE) function
	*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	/*
	2. Configure the TIM pins by configuring the corresponding GPIO pins
	*/
	GPIO_InitTypeDef  GPIO_InitStructure;

	/* Configure the GPIO_LED pin */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_TIM4);
	/*
	2. Configure the Time base unit as described in the first part of this driver,
	  if needed, else the Timer will run with the default configuration:
	  - Autoreload value = 0xFFFF
	  - Prescaler value = 0x0000
	  - Counter mode = Up counting
	  - Clock Division = TIM_CKD_DIV1
	*/

	TIM_TimeBaseInitTypeDef timInit;
	timInit.TIM_Prescaler=0; //FIXME eigentlich 0
	timInit.TIM_CounterMode=TIM_CounterMode_Up;
	timInit.TIM_Period=0xFFFFFFFF;
	timInit.TIM_ClockDivision=TIM_CKD_DIV1; //FIXME eigentlich TIM_CKD_DIV1
	TIM_TimeBaseInit(TIM4, &timInit);
	/*
	3. Fill the TIM_OCInitStruct with the desired parameters including:
	  - The TIM Output Compare mode: TIM_OCMode
	  - TIM Output State: TIM_OutputState
	  - TIM Pulse value: TIM_Pulse
	  - TIM Output Compare Polarity : TIM_OCPolarity

	4. Call TIM_OCxInit(TIMx, &TIM_OCInitStruct) to configure the desired channel with the
	  corresponding configuration
	*/
	TIM_OCInitTypeDef ocInit;

	ocInit.TIM_OCMode=TIM_OCMode_Active;
	ocInit.TIM_OutputState=TIM_OutputState_Enable;
	ocInit.TIM_Pulse=0x2000;
	ocInit.TIM_OCPolarity=TIM_OCPolarity_Low; //FIXME eigentlich TIM_OCPolarity_Low

	TIM_OC3Init(TIM4,&ocInit);
	/*
	5. Call the TIM_Cmd(ENABLE) function to enable the TIM counter.
	*/
	TIM_Cmd(TIM4,ENABLE);
	/*
	Note1: All other functions can be used separately to modify, if needed,
		  a specific feature of the Timer.

	Note2: In case of PWM mode, this function is mandatory:
		  TIM_OCxPreloadConfig(TIMx, TIM_OCPreload_ENABLE);

	Note3: If the corresponding interrupt or DMA request are needed, the user should:
			1. Enable the NVIC (or the DMA) to use the TIM interrupts (or DMA requests).
			2. Enable the corresponding interrupt (or DMA request) using the function
			   TIM_ITConfig(TIMx, TIM_IT_CCx) (or TIM_DMA_Cmd(TIMx, TIM_DMA_CCx))
	 */

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	//Besser noch nicht einschalten. Die Last ist zu hoch!
	//TIM_ITConfig(TIM4,TIM_IT_CC3,ENABLE);

}

void mfm_read_init()
{
/*
	   ===================================================================
			  TIM Driver: how to use it in Input Capture Mode
	   ===================================================================
	   To use the Timer in Input Capture mode, the following steps are mandatory:

	   1. Enable TIM clock using RCC_APBxPeriphClockCmd(RCC_APBxPeriph_TIMx, ENABLE) function
*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
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
	//Besser noch nicht einschalten. Die Last ist zu hoch!
	//TIM_ITConfig(TIM2,TIM_IT_CC3,ENABLE);
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
#endif


void mfm_read_setEnableState(FunctionalState state)
{
	TIM2->CNT=0;
	TIM_ITConfig(TIM2,TIM_IT_CC3,state);
}


void mfm_write_setEnableState(FunctionalState state)
{
	lastCompare=0;
	TIM4_IRQHandler();
	TIM4->CNT=0;
	TIM_ITConfig(TIM4,TIM_IT_CC3,state);
	//printf("Enable %d\n",TIM4->CNT);

}


//Wichtig für den Unit Test, um während des Wartens Inputs zu liefern.
#ifdef CUNIT

	void activeWaitCbk(void);

	#define ACTIVE_WAITING activeWaitCbk();
#else
	#define ACTIVE_WAITING
#endif



void mfm_blockedWaitForSyncWord(int expectNum)
{
	mfm_timeOut=400000;

	//STM_EVAL_LEDOff(LED4);
	while (mfm_inSync!=expectNum && mfm_timeOut)
	{
		ACTIVE_WAITING
		mfm_timeOut--;
	}

	if (mfm_inSync!=expectNum)
		mfm_errorHappened=1;

	/*
	else
	{
		STM_EVAL_LEDOn(LED4);
	}
	*/
}


void mfm_blockedRead()
{
	mfm_timeOut=30000;
	mfm_decodedByteValid=0;

	while (!mfm_decodedByteValid && mfm_timeOut)
	{
		ACTIVE_WAITING
		mfm_timeOut--;
	}

	if (!mfm_decodedByteValid)
		mfm_errorHappened=1;
}

void mfm_blockedWrite(uint32_t word)
{
	mfm_write_nextWord=word;
	mfm_write_busy=1;
	while (mfm_write_busy)
	{
		ACTIVE_WAITING
	}
	//printf("mfm_blockedWrite %x finish\n",word);
}


void mfm_configureWrite(enum mfmEncodeMode mode, int wordLen)
{
	mfm_write_nextWord_len=wordLen;
	mfm_write_nextWord_encodeMode=mode;

	if (mfm_write_nextWord_encodeMode==MFM_ENCODE_ODD)
	{
		mfm_write_nextWord_mask=1<<(wordLen*2-1);
		//printf("mfm_write_nextWord_mask %lx\n",mfm_write_nextWord_mask);
	}
	else
		mfm_write_nextWord_mask=1<<(wordLen-1);
}


