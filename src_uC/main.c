#include <stdio.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "floppy_sector.h"
#include "floppy_control.h"
#include "floppy_mfm.h"


volatile int systickCnt=0;
volatile char printVal=0;
void SysTick_Handler(void)
{
	systickCnt++;

	printVal=1;

	if ((systickCnt%10)==0)
	{
		STM_EVAL_LEDOn(LED3);
		STM_EVAL_LEDOn(LED4);

		STM_EVAL_LEDOff(LED5);
		STM_EVAL_LEDOff(LED6);


	}
	else
	{
		STM_EVAL_LEDOff(LED3);
		STM_EVAL_LEDOff(LED4);

		STM_EVAL_LEDOn(LED5);
		STM_EVAL_LEDOn(LED6);
	}
}


void led_init()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	/* GPIOD clock enable */
	STM_EVAL_LEDInit(LED3);
	STM_EVAL_LEDInit(LED4);
	STM_EVAL_LEDInit(LED5);
	STM_EVAL_LEDInit(LED6);

	STM_EVAL_LEDOn(LED3);
	STM_EVAL_LEDOn(LED4);
	STM_EVAL_LEDOff(LED5);
	STM_EVAL_LEDOff(LED6);
}

struct pt floppy_sectorRead_thread_pt;
struct pt floppyControl_step_thread_pt;

/*
extern volatile unsigned int diffCollector[1000];
extern volatile unsigned int diffCollector_Anz;
volatile unsigned int diffCollector_printIt=0;
*/

void main()
{
	SysTick_Config(SystemCoreClock/ 100);
	led_init();
	mfm_init();
	floppyControl_init();

	printf("Slamy STM32 Floppy Controller\n");

	PT_INIT(&floppy_sectorRead_thread_pt);

	floppyControl_selectDrive(DRIVE_SELECT_A);
	floppyControl_setMotor(0,1);
	mfm_setBitTime(MFM_BITTIME_HD);
	//activeWaitMeasure();

	for(;;)
	{
		PT_SCHEDULE(floppy_sectorRead_thread(&floppy_sectorRead_thread_pt));
		PT_SCHEDULE(floppyControl_step_thread(&floppyControl_step_thread_pt));

		if (printVal)
		{
			printVal=0;
			//printf("%d\n",minDiff);
			//printf("CaptureVal:%d %d\n",TIM_GetCapture3(TIM2),TIM_GetCounter(TIM2));

		}

		/*
		if (diffCollector_Anz==999 && diffCollector_printIt!=999)
		{
			printf("%d\n",diffCollector[diffCollector_printIt]);

			if (diffCollector_printIt<999)
				diffCollector_printIt++;
		}
		*/
	}
}

