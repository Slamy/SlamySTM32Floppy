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


/*
void gpio_setPin4Mode(GPIO_TypeDef* GPIOx,GPIOMode_TypeDef GPIO_Mode)
{
	GPIOx->MODER  &= ~(GPIO_MODER_MODER0 << 8);
	GPIOx->MODER |= (((uint32_t)GPIO_Mode) << 8);
}
*/

void gpio_setPin1Mode(GPIO_TypeDef* GPIOx,GPIOMode_TypeDef GPIO_Mode)
{
	GPIOx->MODER  &= ~(GPIO_MODER_MODER0 << 2);
	GPIOx->MODER |= (((uint32_t)GPIO_Mode) << 2);
}


void floppyControl_init()
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	//set the pins high to make them inactive before activation
	GPIOB->BSRRL=GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_11;
	GPIOA->BSRRL=GPIO_Pin_8 | GPIO_Pin_15;

	//Init Input GPIOs

	//Init /INDEX
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//Init /TRK00
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	//Init Output GPIOs

	//Init /REDWC, /MOTEA, /DRVSB
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//Init /DRVSA, /MOTEB, /DIR, /STEP, /SIDE1, /WGATE
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);


	//init TIM3. TIM3 is used for time measuring of timeouts and stepping.
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	TIM_TimeBaseInitTypeDef timInit;
	timInit.TIM_Prescaler=4000;
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

	//init index interrupt. The used pin is PA3.

	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource3);

	EXTI_InitTypeDef EXTI_InitStruct;
	EXTI_StructInit(&EXTI_InitStruct);

	EXTI_InitStruct.EXTI_Line = EXTI_Line3;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;

	EXTI_Init(&EXTI_InitStruct);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

}

void floppy_selectDensity(enum Density val)
{
	if (val==DENSITY_DOUBLE)
	{
		//GPIOA->BSRRL=GPIO_Pin_1; //set /REDWC to high. High Density
		gpio_setPin1Mode(GPIOA,GPIO_Mode_IN);
		printf("set /REDWC to high. Makes it low/double density\n");
	}
	else
	{
		GPIOA->BSRRH=GPIO_Pin_1; //set /REDWC to low. Double Density
		gpio_setPin1Mode(GPIOA,GPIO_Mode_OUT);
		printf("set /REDWC to low. Makes it high density.\n");
	}
}

void floppy_selectDrive(enum DriveSelect sel)
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

void floppy_setMotor(int drive, int val)
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

void floppy_setHead(int head)
{

	//printf("setHead %d %d\n",GPIOB->IDR & GPIO_Pin_11,head);
	if (head)
		GPIOB->BSRRH = GPIO_Pin_11; //set /SIDE1 low -> head 1
	else
		GPIOB->BSRRL = GPIO_Pin_11; //set /SIDE1 high -> head 0
	//printf(" %d\n",GPIOB->IDR & GPIO_Pin_11);
}

#define STEP_WAIT_TIME 125
#define STEP_LOW_TIME 13

void setupStepTimer(int waitTime)
{
	TIM3->CNT=0;
	TIM3->CCR1=waitTime;
	TIM_ClearFlag(TIM3,TIM_FLAG_CC1);
}

static unsigned int currentTrack=0;

void floppy_stepToCylinder00()
{
	int trys=0;

	GPIOB->BSRRL=GPIO_Pin_2; //set /DIR to high to step outside

	while ((GPIOB->IDR & GPIO_Pin_7))
	{
		//printf("ST OUT! %d %d\n",TIM3->CNT,TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1));

		setupStepTimer(STEP_WAIT_TIME);
		while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		GPIOB->BSRRH=GPIO_Pin_4; //set /STEP to low

		setupStepTimer(STEP_LOW_TIME);
		while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		GPIOB->BSRRL=GPIO_Pin_4; //set /STEP to high
		trys++;

		if (trys > 120)
		{
			printf("floppy_stepToTrack00() TIMEOUT\n");
			return;
		}
	}

	currentTrack=0;
}

void floppy_stepToCylinder(unsigned int wantedCyl)
{
	while (currentTrack != wantedCyl)
	{
		/*
		if (!(GPIOB->IDR & GPIO_Pin_7))
		{
			currentTrack=0;
		}
		*/

		//printf("TRK00:%x\n",GPIOB->IDR & GPIO_Pin_7);

		//PT_WAIT_UNTIL(pt,currentTrack != wantedTrack);

		if (currentTrack > wantedCyl)
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
		while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		GPIOB->BSRRH=GPIO_Pin_4; //set /STEP to low
		//gpio_setPin4Mode(GPIOB,GPIO_Mode_OUT);

		setupStepTimer(STEP_LOW_TIME);
		while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		GPIOB->BSRRL=GPIO_Pin_4; //set /STEP to high
		//gpio_setPin4Mode(GPIOB,GPIO_Mode_IN);
	}

}

void floppy_setWriteGate(int val)
{
	//printf("setWriteGate %d",GPIOB->IDR & GPIO_Pin_5);
	if (val)
		GPIOB->BSRRH=GPIO_Pin_5; //set /WGATE to low
	else
		GPIOB->BSRRL=GPIO_Pin_5; //set /WGATE to high

	//printf(" %d %d\n",val,GPIOB->IDR & GPIO_Pin_5);
}



volatile unsigned int indexHappened=0;

void EXTI3_IRQHandler(void)
{

	if(EXTI_GetITStatus(EXTI_Line3) != RESET)
	{
		indexHappened=1;
		//printf("I\n");

		/* Clear the EXTI line pending bit */
		EXTI_ClearITPendingBit(EXTI_Line3);
	}
	/*
	else
	{
		//printf("Unexpected EXTI3!\n");
	}
	*/
}


int floppy_waitForIndex()
{
	//Wir wollen hier auf den Moment der fallenden Flanke warten.

	setupStepTimer(50000);

	indexHappened=0;
	while (!indexHappened)
	{
		if (TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
		{
			printf("Index Timeout\n");
			return 1;
		}
	}
	indexHappened=0;

	return 0;
}


void floppy_measureRpm()
{
	int i;

	if (floppy_waitForIndex()) //das erste mal um synchron zu sein.
	{
		printf("Index Timeout!\n");
		return;
	}

	setupStepTimer(50000);

	for (i=0;i<10;i++)
	{
		indexHappened=0;
		while (!indexHappened)
		{
			if (TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
			{
				printf("TimeOut\n");
				return;
			}
		}

		uint16_t cnt=TIM3->CNT;
		setupStepTimer(50000);

		float rpm=60.0f * 1000000.0f / ( 47.619047619f * (float)cnt);
		printf("%.1f RPM\n",rpm);
	}
}



