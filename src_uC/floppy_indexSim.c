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
#include "floppy_indexSim.h"

volatile static uint32_t lastCompare=0;
volatile static uint16_t wasteTime=0;

int indexDiodeState=0;


#define RPM360_TIM5_TICKS	14005000	//Eine Umdrehung bei 360 RPM dauert 14000000 Zyklen
#define RPM360_PULSE_LEN	38888		//In der Annahme, dass der Pulse ein 1° von der Disk ausmacht


//#define RPM360_TIM5_TICKS	134000000
//#define RPM360_PULSE_LEN	20000000

void TIM5_IRQHandler(void)
{
	//Wenn state==0, dann wurde der Pin jetzt high. Es muss auf Inaktiv geschaltet werden.

	//printf("I5\n");

	TIM_ClearITPendingBit(TIM5, TIM_IT_CC2);


	if (indexDiodeState)
		lastCompare+=(RPM360_TIM5_TICKS-RPM360_PULSE_LEN);
	else
		lastCompare+=RPM360_PULSE_LEN;

	//lastCompare+=14000000; //genau 166666 µS. 1/360 Sekunde.


	//lastCompare+=0x10000000;
	//printf("TIM5 %d\n",lastCompare);
	TIM_SetCompare2(TIM5,lastCompare);



	/* Get the current value of the output compare mode */
	uint16_t tmpccmr1 = 0;
	tmpccmr1 = TIM5->CCMR1;

	/* Reset the OC1M Bits */
	tmpccmr1 &= (uint16_t)~TIM_CCMR1_OC2M;

	/* Configure The Active output Mode */
	//TIM_CCMR1_OC2M_1 ist Inaktiv bei Compare
	//TIM_CCMR1_OC2M_0 ist Aktiv bei Compare
	tmpccmr1 |= indexDiodeState ? TIM_CCMR1_OC2M_0 : TIM_CCMR1_OC2M_1;

	/* Write to TIMx CCMR2 register */
	TIM5->CCMR1 = tmpccmr1;

	indexDiodeState=!indexDiodeState;
}


#ifndef CUNIT


void floppy_indexSim_init()
{
	/*
	===================================================================
		  TIM Driver: how to use it in Output Compare Mode
	===================================================================
	To use the Timer in Output Compare mode, the following steps are mandatory:

	1. Enable TIM clock using RCC_APBxPeriphClockCmd(RCC_APBxPeriph_TIMx, ENABLE) function
	*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	/*
	2. Configure the TIM pins by configuring the corresponding GPIO pins
	*/
	GPIO_InitTypeDef  GPIO_InitStructure;

	/* Configure the GPIO_LED pin */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_TIM5);
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
	timInit.TIM_ClockDivision=TIM_CKD_DIV1; //FIXME eigentlich TIM_CKD_DIV1
	TIM_TimeBaseInit(TIM5, &timInit);
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
	ocInit.TIM_OCPolarity=TIM_OCPolarity_High;

	TIM_OC2Init(TIM5,&ocInit);
	/*
	5. Call the TIM_Cmd(ENABLE) function to enable the TIM counter.
	*/
	//TIM_Cmd(TIM5,ENABLE);
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
	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	indexDiodeState=0;
	//TIM_ITConfig(TIM4,TIM_IT_CC3,ENABLE); //provisorisch schalten wir es ein. Je nach Config wird es ohnehin deaktiviert

}

#endif



void floppy_indexSim_setEnableState(FunctionalState state)
{
	printf("floppy_indexSim_setEnableState %d\n",state);
	if (state==ENABLE)
	{
		lastCompare=0;
		TIM5_IRQHandler();
		TIM5->CNT=0;
		TIM_Cmd(TIM5,ENABLE);
		TIM_ITConfig(TIM5,TIM_IT_CC2,state);


	}
	else
	{
		TIM_Cmd(TIM5,DISABLE);
		TIM_ITConfig(TIM5,TIM_IT_CC2,state);

		//Nur ausschalten reicht nicht. Auch dauerhaft den Transistor deaktivieren.

		/* Get the current value of the output compare mode */
		uint16_t tmpccmr1 = 0;
		tmpccmr1 = TIM5->CCMR1;

		/* Reset the OC1M Bits */
		tmpccmr1 &= (uint16_t)~TIM_CCMR1_OC2M;

		/* Configure The Active output Mode */
		//TIM_CCMR1_OC2M_0 ist Aktiv bei Compare
		//TIM_CCMR1_OC2M_1 ist Inaktiv bei Compare
		//TIM_CCMR1_OC2M_2 ist Force Inaktiv
		tmpccmr1 |= TIM_CCMR1_OC2M_2;

		/* Write to TIMx CCMR2 register */
		TIM5->CCMR1 = tmpccmr1;
	}

}



