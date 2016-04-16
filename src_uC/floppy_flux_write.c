/*
 * floppy_mfm_write.c
 *
 *  Created on: 03.04.2016
 *      Author: andre
 */

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_mfm.h"
#include "floppy_control.h"


static volatile uint32_t flux_write_nextWord=0;
static volatile uint32_t flux_write_nextWord_mask=0x80;
static volatile enum fluxEncodeMode flux_write_nextWord_encodeMode=0;
volatile uint32_t flux_write_nextWord_len=0;
volatile uint16_t flux_write_nextWord_cellLength=0;
volatile uint16_t flux_write_nextWord_extraPause=0;

static volatile uint32_t flux_write_busy=0;

static volatile uint32_t flux_write_currentWord_mask=0x80;
static volatile uint32_t flux_write_currentWord_len=0;
static volatile uint32_t flux_write_currentWord_bit=0;
static volatile uint32_t flux_write_currentWord=0;
static volatile uint16_t flux_write_currentWord_cellLength=0;
static volatile uint16_t flux_write_currentWord_extraPause=0;
static volatile enum fluxEncodeMode flux_write_currentWord_encodeMode=0;

static volatile uint32_t flux_write_lastBit=0;

volatile int debug_addedExtraPauses=0;

uint16_t flux_write_calcNextPauseLen(void)
{
	static uint16_t pauseLenAccu=0;
	uint16_t pauseLenRet=0;

	pauseLenAccu+=flux_write_currentWord_extraPause;
	flux_write_currentWord_extraPause=0;

	/*
	printf("mfm_write_calcNextPauseLen %x %d %d %x\n",mfm_write_currentWord,
											mfm_write_currentWord_encodeMode,
											mfm_write_currentWord_bit,
											mfm_write_currentWord_mask);
	 */

	//Wenn der Raw Mode aktiv ist und keine 1en existieren, so pausieren wir den ganzen Automaten für ein paar MFM Zellen und machen dann weiter

	if (!flux_write_currentWord_cellLength)
		flux_write_currentWord_cellLength=mfm_decodeCellLength;

	if (flux_write_currentWord_encodeMode == FLUX_RAW && flux_write_currentWord==0)
	{
		flux_write_currentWord_bit=0;
		flux_write_currentWord=flux_write_nextWord;
		flux_write_currentWord_encodeMode=flux_write_nextWord_encodeMode;
		flux_write_currentWord_len=flux_write_nextWord_len;
		flux_write_currentWord_mask=flux_write_nextWord_mask;
		flux_write_currentWord_cellLength=flux_write_nextWord_cellLength;
		flux_write_currentWord_extraPause=flux_write_nextWord_extraPause;
		flux_write_nextWord_extraPause=0;
		flux_write_busy=0;
		return mfm_decodeCellLength*8;
	}

	while (!pauseLenRet) //wir akkumulieren Pausenzeiten, bis eine 1 Transition kommt.
	{

		if (flux_write_currentWord_encodeMode == FLUX_RAW)
		{
			if (flux_write_currentWord & flux_write_currentWord_mask)
			{
				//eine 1 bedeutet eine zelle pause und eine transition
				pauseLenRet=flux_write_currentWord_cellLength + pauseLenAccu;
				pauseLenAccu=0;
			}
			else
			{
				//eine 0 ist eine Pause. Wir müssen noch warten
				pauseLenAccu+=flux_write_currentWord_cellLength;
			}
		}
		else //wenn nicht raw, dann nach ISO encodieren.
		{
			if (flux_write_currentWord & flux_write_currentWord_mask)
			{
				//Eine 1 ist immer 01. 0 ist eine Pause. 1 ist eine Pause mit Transition.
				pauseLenRet=pauseLenAccu + (flux_write_currentWord_cellLength<<1);
				pauseLenAccu=0;
				flux_write_lastBit=1;
			}
			else
			{
				if (flux_write_lastBit)
				{
					//Wenn das letzte Bit eine 1 war, dann brauchen wir hier nichts zu tun. 00. Also 2 Pausen

					pauseLenAccu += flux_write_currentWord_cellLength<<1;
				}
				else
				{
					//Das letzte Bit war schon eine 0. Dann direkt eine Transition NACH einer Pause erzeugen und eine Pause hinten dran.
					pauseLenRet = flux_write_currentWord_cellLength+pauseLenAccu;
					pauseLenAccu = flux_write_currentWord_cellLength;

				}

				flux_write_lastBit = 0;
			}
		}

		flux_write_currentWord<<=1;
		if (flux_write_currentWord_encodeMode == FLUX_MFM_ENCODE_ODD)
			flux_write_currentWord<<=1;

		flux_write_currentWord_bit++;

		if (flux_write_currentWord_bit >= flux_write_currentWord_len)
		{
			flux_write_currentWord_bit=0;
			flux_write_currentWord=flux_write_nextWord;
			flux_write_currentWord_encodeMode=flux_write_nextWord_encodeMode;
			flux_write_currentWord_len=flux_write_nextWord_len;
			flux_write_currentWord_mask=flux_write_nextWord_mask;
			flux_write_currentWord_cellLength=flux_write_nextWord_cellLength;
			flux_write_currentWord_extraPause=flux_write_nextWord_extraPause;

			if (flux_write_nextWord_extraPause && flux_write_busy==0)
			{
				printf("Zu langsam! ************\n");
				printf("debug_addedExtraPauses %d\n",debug_addedExtraPauses);
				assert(0);
			}
			flux_write_nextWord_extraPause=0;
			flux_write_busy=0;
		}
	}

	return pauseLenRet;
}

volatile static uint16_t lastCompare=0;

//volatile static uint16_t wasteTime=0;

void TIM4_IRQHandler(void)
{
	//printf("I\n");

	uint16_t pulseLen=flux_write_calcNextPauseLen();

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


void flux_write_init()
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

#endif




void flux_write_setEnableState(FunctionalState state)
{
	lastCompare=0;
	TIM4_IRQHandler();
	TIM4->CNT=0;
	TIM_ITConfig(TIM4,TIM_IT_CC3,state);
	//printf("Enable %d\n",TIM4->CNT);

}


void flux_blockedWrite(uint32_t word)
{
	if (indexHappened)
		indexOverflowCount++;

#if 0
	if (word==0 && flux_write_currentWord_encodeMode == FLUX_RAW)
	{
		flux_write_nextWord_extraPause+=flux_write_nextWord_cellLength*flux_write_nextWord_len;
		debug_addedExtraPauses++;
		return;
	}
#endif

	//printf("mfm_blockedWrite %x\n",word);
	flux_write_nextWord=word;
	flux_write_busy=1;
	while (flux_write_busy)
	{
		ACTIVE_WAITING
	}
	//printf("mfm_blockedWrite %x finish\n",word);
}


void flux_configureWrite(enum fluxEncodeMode mode, int wordLen)
{
	flux_write_nextWord_len=wordLen;
	flux_write_nextWord_encodeMode=mode;

	if (flux_write_nextWord_encodeMode==FLUX_MFM_ENCODE_ODD)
	{
		flux_write_nextWord_mask=1<<(wordLen*2-1);
		//printf("mfm_write_nextWord_mask %lx\n",mfm_write_nextWord_mask);
	}
	else
		flux_write_nextWord_mask=1<<(wordLen-1);
}

void flux_configureWriteCellLength(uint16_t cellLength)
{
	if (cellLength)
	{
		//printf("mfm_write_nextWord_cellLength %d\n",cellLength);
		flux_write_nextWord_cellLength=cellLength;
	}
	else
		flux_write_nextWord_cellLength=mfm_decodeCellLength;
}


