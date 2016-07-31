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
#include "floppy_flux_write.h"
#include "assert.h"

enum DriveSelect readyDrives;
enum DriveSelect selectedDrive;


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

void gpio_setPin13Mode(GPIO_TypeDef* GPIOx,GPIOMode_TypeDef GPIO_Mode)
{
	GPIOx->MODER  &= ~(GPIO_MODER_MODER0 << 26);
	GPIOx->MODER |= (((uint32_t)GPIO_Mode) << 26);
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

	//Init /MOTEA, /DRVSB
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//Init /DRVSA, /MOTEB, /DIR, /STEP, /SIDE1, /WGATE, /REDWC
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_11 | GPIO_Pin_13;
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
		gpio_setPin13Mode(GPIOB,GPIO_Mode_IN);
		printf("set /REDWC to high. Makes it low/double density\n");
	}
	else
	{
		GPIOA->BSRRH=GPIO_Pin_1; //set /REDWC to low. Double Density
		gpio_setPin13Mode(GPIOB,GPIO_Mode_OUT);
		printf("set /REDWC to low. Makes it high density.\n");
	}
}

void floppy_selectFittingDrive()
{
	//für Testzwecke...
	//floppy_selectDrive(DRIVE_SELECT_A);
	//return;

	//nimm das Laufwerk, das am ehesten passen würde

	//haben wir nur eines dran? dann nimm dieses!
	if (readyDrives != (DRIVE_SELECT_A | DRIVE_SELECT_B))
	{
		floppy_selectDrive(readyDrives);
		return;
	}

	//wenn nicht, gibt es Empfehlungen für eine Diskettengröße ?

	if (preferedFloppyMedium == FLOPPY_MEDIUM_3_5_INCH)
		floppy_selectDrive(DRIVE_SELECT_B);
	else if (preferedFloppyMedium == FLOPPY_MEDIUM_5_1_4_INCH)
		floppy_selectDrive(DRIVE_SELECT_A);
	else
		assert(0);

}

void floppy_selectDrive(enum DriveSelect sel)
{
	selectedDrive=sel;

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
		default:
			assert(0);
			break;
	}
}


void floppy_control_senseDrives()
{
	readyDrives=0;

	floppy_selectDrive(DRIVE_SELECT_A);
	floppy_stepToCylinder(5);
	if (!floppy_stepToCylinder00())
	{
		readyDrives|=DRIVE_SELECT_A;
		printf("Drive A Ready\n");
	}

	floppy_selectDrive(DRIVE_SELECT_B);
	floppy_stepToCylinder(5);
	if (!floppy_stepToCylinder00())
	{
		readyDrives|=DRIVE_SELECT_B;
		printf("Drive B Ready\n");
	}

	floppy_selectDrive(DRIVE_SELECT_NONE);
}

void floppy_setMotorSelectedDrive(int val)
{
	if (val)
	{
		assert (selectedDrive != DRIVE_SELECT_NONE);
		if (selectedDrive == DRIVE_SELECT_A)
		{
			GPIOB->BSRRL=GPIO_Pin_1; //set /MOTEB to high -> deactivate motor of drive B
			GPIOA->BSRRH=GPIO_Pin_8; //set /MOTEA to low -> activate motor of drive A
			printf("Enable Motor of Drive A\n");
		}
		else if (selectedDrive == DRIVE_SELECT_B)
		{
			GPIOA->BSRRL=GPIO_Pin_8; //set /MOTEA to high -> deactivate motor of drive A
			GPIOB->BSRRH=GPIO_Pin_1; //set /MOTEB to low -> activate motor of drive B
			printf("Enable Motor of Drive B\n");
		}
		else
			assert(0);

	}
	else
	{
		GPIOB->BSRRL=GPIO_Pin_1; //set /MOTEB to high -> deactivate motor of drive B
		GPIOA->BSRRL=GPIO_Pin_8; //set /MOTEA to high -> deactivate motor of drive A
	}
}

void floppy_setMotor(enum DriveSelect drive, int val)
{
	if (drive == DRIVE_SELECT_B)
	{
		//B
		if (!val)
			GPIOB->BSRRL=GPIO_Pin_1; //set /MOTEB to high -> deactivate motor of drive B
		else
			GPIOB->BSRRH=GPIO_Pin_1; //set /MOTEB to low -> activate motor of drive B
	}
	else if (drive == DRIVE_SELECT_A)
	{
		//A
		if (!val)
			GPIOA->BSRRL=GPIO_Pin_8; //set /MOTEA to high -> deactivate motor of drive A
		else
			GPIOA->BSRRH=GPIO_Pin_8; //set /MOTEA to low -> activate motor of drive A
	}
	else
		assert(0);
}

void floppy_setHead(int head)
{
	/*
	if (configuration_flags & CONFIGFLAG_INVERT_SIDES)
		head=!head;
	*/

	//printf("setHead %d %d\n",GPIOB->IDR & GPIO_Pin_11,head);
	if (head)
		GPIOB->BSRRH = GPIO_Pin_11; //set /SIDE1 low -> head 1
	else
		GPIOB->BSRRL = GPIO_Pin_11; //set /SIDE1 high -> head 0
	//printf(" %d\n",GPIOB->IDR & GPIO_Pin_11);
}

#define STEP_SETTLE_TIME 350
#define STEP_WAIT_TIME 125
#define STEP_LOW_TIME 13

void setupStepTimer(int waitTime)
{
	TIM3->CNT=0;
	TIM3->CCR1=waitTime;
	TIM_ClearFlag(TIM3,TIM_FLAG_CC1);
}

static unsigned int currentTrack=0;

int floppy_stepToCylinder00()
{
	int trys=0;

	GPIOB->BSRRL=GPIO_Pin_2; //set /DIR to high to step outside

	while ((GPIOB->IDR & GPIO_Pin_7))
	{
		//printf("ST OUT! %d %d\n",TIM3->CNT,TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1));

		GPIOB->BSRRH=GPIO_Pin_4; //set /STEP to low

		setupStepTimer(STEP_LOW_TIME);
		while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		GPIOB->BSRRL=GPIO_Pin_4; //set /STEP to high
		trys++;

		setupStepTimer(STEP_WAIT_TIME);
		while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		if (trys > 85)
		{
			printf("floppy_stepToTrack00() TIMEOUT\n");
			return 1;
		}
	}

	setupStepTimer(STEP_SETTLE_TIME);
	while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

	currentTrack=0;
	return 0;
}

int floppy_stepToCylinder00Tested()
{
	int ret=0;

	while (currentTrack != 0)
	{
		/*
		if (!(GPIOB->IDR & GPIO_Pin_7))
		{
			currentTrack=0;
		}
		*/

		//printf("TRK00:%x\n",GPIOB->IDR & GPIO_Pin_7);

		//PT_WAIT_UNTIL(pt,currentTrack != wantedTrack);

		//printf("ST OUT! %d\n",SysTick->VAL);
		GPIOB->BSRRL=GPIO_Pin_2; //set /DIR to high to step outside
		currentTrack--;

		setupStepTimer(STEP_WAIT_TIME);
		while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		if (!(GPIOB->IDR & GPIO_Pin_7)) //is /TRK00 low?
		{
			printf("***  /TRK00 ist unerlaubt low\n");
			ret=1;
		}

		GPIOB->BSRRH=GPIO_Pin_4; //set /STEP to low
		//gpio_setPin4Mode(GPIOB,GPIO_Mode_OUT);

		setupStepTimer(STEP_LOW_TIME);
		while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

		GPIOB->BSRRL=GPIO_Pin_4; //set /STEP to high
		//gpio_setPin4Mode(GPIOB,GPIO_Mode_IN);
	}

	setupStepTimer(STEP_SETTLE_TIME);
	while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

	if ((GPIOB->IDR & GPIO_Pin_7)) //is /TRK00 high?
	{
		printf("***  /TRK00 ist unerlaubt high\n");
		ret=1;
	}

	return ret;
}

void floppy_steppingTest()
{
	int ret=0;

	printf("Step to Cyl 20 and reset for clean test start\n");
	floppy_stepToCylinder(20);
	ret|=floppy_stepToCylinder00();

	setupStepTimer(20000);
	while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

	printf("Step to Cyl 1 and back\n");
	floppy_stepToCylinder(1);
	ret|=floppy_stepToCylinder00Tested();

	setupStepTimer(20000);
	while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

	printf("Step to Cyl 3 and back\n");
	floppy_stepToCylinder(3);
	ret|=floppy_stepToCylinder00Tested();

	setupStepTimer(20000);
	while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

	printf("Step slowly forward and back... now step\n");
	floppy_stepToCylinder(1);

	setupStepTimer(20000);
	while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

	printf("and step\n");
	floppy_stepToCylinder(2);

	setupStepTimer(20000);
	while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);

	printf("and back\n");
	ret|=floppy_stepToCylinder00Tested();

	printf("Now for hardcore!\n");
	floppy_stepToCylinder(20);
	ret|=floppy_stepToCylinder00Tested();
	floppy_stepToCylinder(30);
	ret|=floppy_stepToCylinder00Tested();
	floppy_stepToCylinder(40);
	ret|=floppy_stepToCylinder00Tested();
	floppy_stepToCylinder(50);
	ret|=floppy_stepToCylinder00Tested();
	floppy_stepToCylinder(60);
	ret|=floppy_stepToCylinder00Tested();

	if (ret)
		printf("Stepping test FAILED!\n");
	else
		printf("Stepping test SUCCEEDED!\n");

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

	setupStepTimer(STEP_SETTLE_TIME);
	while(TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==RESET);
}

void floppy_setWriteGate(int val)
{

	//val=0; //Kein schreiben zulassen

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

#if 1
	setupStepTimer(20000);

	while (!indexHappened)
	{
		if (TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
		{
			return 0;
		}
	}


#endif

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
	indexOverflowCount=0;

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



