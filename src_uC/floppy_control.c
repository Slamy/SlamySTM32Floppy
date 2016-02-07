/*
 * floppy_control.c
 *
 *  Created on: 06.02.2016
 *      Author: andre
 */

#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "floppy_sector.h"
#include "floppy_control.h"
#include "floppy_mfm.h"


void floppyControl_init()
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);


	GPIOB->BSRRL=GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_4; //set the pins high to make them inactive!
	GPIOA->BSRRL=GPIO_Pin_8 | GPIO_Pin_15;

	//Init /DRVSA, /MOTEB, /DIR and /STEP
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	//Init /TRK00

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);


	//Init /MOTEA and /DRVSB
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);



	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	TIM_TimeBaseInitTypeDef timInit;
	timInit.TIM_Prescaler=500;
	timInit.TIM_CounterMode=TIM_CounterMode_Up;
	timInit.TIM_Period=0xFFFFFFFF;
	timInit.TIM_ClockDivision=TIM_CKD_DIV1;
	timInit.TIM_RepetitionCounter=0x50;
	TIM_TimeBaseInit(TIM3, &timInit);

	TIM_OCInitTypeDef TIM_OCInitStruct;
	TIM_OCInitStruct.TIM_OCMode=TIM_OCMode_Timing;
	TIM_OCInitStruct.TIM_OutputState=TIM_OutputState_Enable;
	TIM_OCInitStruct.TIM_Pulse=9000;

    TIM_OC1Init(TIM3, &TIM_OCInitStruct);

	TIM_Cmd(TIM3,ENABLE);


}



void floppyControl_selectDrive(enum DriveSelect sel)
{
	switch (sel)
	{
		case DRIVE_SELECT_NONE:
			GPIOB->BSRRL=GPIO_Pin_0; //set /DRVSA to high -> deselect drive A
			GPIOA->BSRRL=GPIO_Pin_15; //set /DRVSB to high -> deselect drive B
			break;
		case DRIVE_SELECT_A:
			GPIOB->BSRRH=GPIO_Pin_0; //set /DRVSA to low -> select drive A
			GPIOA->BSRRL=GPIO_Pin_15; //set /DRVSB to high -> deselect drive B
			break;
		case DRIVE_SELECT_B:
			GPIOB->BSRRL=GPIO_Pin_0; //set /DRVSA to high -> deselect drive A
			GPIOA->BSRRH=GPIO_Pin_15; //set /DRVSB to low -> select drive B
			break;
	}
}

void floppyControl_setMotor(int drive, int val)
{
	if (drive)
	{
		//B
		if (val)
		{

		}
	}
	else
	{
		//A
		if (!val)
			GPIOA->BSRRL=GPIO_Pin_8; //set /MOTEA to high -> deactivate motor of drive A
		else
			GPIOA->BSRRH=GPIO_Pin_8; //set /MOTEA to low -> activate motor of drive A
	}
}

unsigned int wantedTrack=30;

#define STEP_WAIT_TIME 1000
#define STEP_LOW_TIME 100

void gpio_setPinMode(GPIO_TypeDef* GPIOx,GPIOMode_TypeDef GPIO_Mode)
{
	GPIOx->MODER  &= ~(GPIO_MODER_MODER0 << 8);
	GPIOx->MODER |= (((uint32_t)GPIO_Mode) << 8);
}

void setupStepTimer(int waitTime)
{
	TIM3->CNT=0;
	TIM3->CCR1=waitTime;
	TIM_ClearFlag(TIM3,TIM_FLAG_CC1);
}

PT_THREAD(floppyControl_step_thread(struct pt *pt))
{
	static unsigned int currentTrack=0;
	static unsigned int stepTime=0;

	PT_BEGIN(pt);

	GPIOB->BSRRL=GPIO_Pin_2; //set /DIR to high to step outside
	while ((GPIOB->IDR & GPIO_Pin_7))
	{
		//printf("ST OUT! %d %d\n",TIM3->CNT,TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1));

		GPIOB->BSRRH=GPIO_Pin_4; //set /STEP to low

		setupStepTimer(STEP_LOW_TIME);
		PT_WAIT_WHILE(pt,TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		GPIOB->BSRRL=GPIO_Pin_4; //set /STEP to high

		setupStepTimer(STEP_WAIT_TIME);
		PT_WAIT_WHILE(pt,TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

	}

	printf("Aligned to Track 00\n");

	for(;;)
	{
		/*
		if (!(GPIOB->IDR & GPIO_Pin_7))
		{
			currentTrack=0;
		}
		*/
		//printf("TRK00:%x\n",GPIOB->IDR & GPIO_Pin_7);

		PT_WAIT_UNTIL(pt,currentTrack != wantedTrack);

		if (currentTrack > wantedTrack)
		{
			//printf("ST OUT! %d\n",SysTick->VAL);
			GPIOB->BSRRL=GPIO_Pin_2; //set /DIR to high to step outside
			currentTrack--;
		}
		else
		{
			//printf("ST IN! %d\n",SysTick->VAL);
			GPIOB->BSRRH=GPIO_Pin_2; //set /DIR to low to step inside
			currentTrack++;
		}

		setupStepTimer(STEP_WAIT_TIME);
		PT_WAIT_WHILE(pt,TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		GPIOB->BSRRH=GPIO_Pin_4; //set /STEP to low
		//gpio_setPinMode(GPIOB,GPIO_Mode_OUT);

		setupStepTimer(STEP_LOW_TIME);
		PT_WAIT_WHILE(pt,TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		GPIOB->BSRRL=GPIO_Pin_4; //set /STEP to high
		//gpio_setPinMode(GPIOB,GPIO_Mode_IN);
	}

	PT_END(pt);
}


