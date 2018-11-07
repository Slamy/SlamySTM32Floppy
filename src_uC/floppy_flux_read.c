/*
 * floppy_flux_read.c
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

unsigned int diff;

#ifdef ACTIVATE_DIFFCOLLECTOR
volatile unsigned short diffCollector[DIFF_COLLECTOR_SIZE];
volatile unsigned int diffCollector_Anz=0;
volatile unsigned int diffCollectorEnabled=0;
#endif

void TIM2_IRQHandler(void)
{
	static unsigned int lastTransitionTime=0;

	//unsigned int intStart=TIM_GetCounter(TIM2);

	unsigned int transitionTime=TIM_GetCapture3(TIM2);
	diff=transitionTime - lastTransitionTime;
	TIM_ClearITPendingBit(TIM2, TIM_IT_CC3);

#ifdef ACTIVATE_DIFFCOLLECTOR
	if (diffCollectorEnabled && diffCollector_Anz < DIFF_COLLECTOR_SIZE)
	{
		diffCollector[diffCollector_Anz++]=diff;
	}
#endif

	if (flux_mode==FLUX_MODE_MFM_AMIGA)
		mfm_amiga_transitionHandler();
	else if (flux_mode==FLUX_MODE_MFM_ISO)
		mfm_iso_transitionHandler();
	else if (flux_mode==FLUX_MODE_GCR_C64)
		gcr_c64_transitionHandler();

	//printf("%u\n",diff);

	lastTransitionTime=transitionTime;
}





#ifndef CUNIT


void flux_read_init()
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


void flux_read_setEnableState(FunctionalState state)
{
	TIM2->CNT=0;
	TIM_ITConfig(TIM2,TIM_IT_CC3,state);
}
