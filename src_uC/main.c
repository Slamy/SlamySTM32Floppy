#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "floppy_sector.h"
#include "floppy_control.h"
#include "floppy_mfm.h"
#include "tm_stm32f4_usb_vcp.h"

volatile int systickCnt=0;
volatile int floppySpinTimeOut=0;
volatile char printVal=0;


void SysTick_Handler(void)
{
	systickCnt++;

	if (floppySpinTimeOut)
		floppySpinTimeOut--;

	printVal=1;

	/*
	if ((systickCnt%10)==0)
	{
		STM_EVAL_LEDOn(LED3);
		STM_EVAL_LEDOn(LED4);

		STM_EVAL_LEDOff(LED5);
		STM_EVAL_LEDOff(LED6);

		//printf("%d %d %d\n",USB_Tx_State,APP_Rx_ptr_in,APP_Rx_ptr_out);
	}
	else
	{
		STM_EVAL_LEDOff(LED3);
		STM_EVAL_LEDOff(LED4);

		STM_EVAL_LEDOn(LED5);
		STM_EVAL_LEDOn(LED6);
	}
	*/
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

/*
extern volatile unsigned int diffCollector[1000];
extern volatile unsigned int diffCollector_Anz;
volatile unsigned int diffCollector_printIt=0;
*/

unsigned char cmd[20];
unsigned int cmdPos=0;
unsigned char c;

volatile unsigned char usb_recv_data[64];
volatile unsigned char usb_recv_len=0;

extern unsigned char trackBuffer[SECTOR_SIZE * MAX_SECTORS_PER_CYLINDER];


void main()
{
	unsigned char *usb_send_data;
	int i,j;
	SysTick_Config(SystemCoreClock/ 100);
	led_init();
	mfm_init();
	floppyControl_init();

	printf("Slamy STM32 Floppy Controller\n");

	TM_USB_VCP_Init();
	//PT_INIT(&floppy_sectorRead_thread_pt);
	floppy_selectDrive(DRIVE_SELECT_A);
	floppy_stepToTrack00();
	floppy_selectDrive(DRIVE_SELECT_NONE);
	//floppyControl_setMotor(0,1);
	//mfm_setBitTime(MFM_BITTIME_HD);
	//transmitTrack(0,18);

	printf("At track 00\n");

	/*
	floppy_selectDrive(DRIVE_SELECT_A);
	floppy_setMotor(0,1);
	floppy_debugTrackDataMachine(0,0);
	floppy_debugTrackDataMachine(0,1);
	floppy_debugTrackDataMachine(1,0);
	floppy_debugTrackDataMachine(1,1);
	*/

	floppySpinTimeOut=500;

	//floppyControl_selectDrive(DRIVE_SELECT_NONE);
	//activeWaitMeasure();

	for(;;)
	{
		//PT_SCHEDULE(floppy_trackRead_thread(&floppy_sectorRead_thread_pt,20));
		//PT_SCHEDULE(floppyControl_step_thread(&floppyControl_step_thread_pt));

		if (printVal)
		{
			printVal=0;
			//printf("%d\n",minDiff);
			//printf("CaptureVal:%d %d\n",TIM_GetCapture3(TIM2),TIM_GetCounter(TIM2));

		}

		if (floppySpinTimeOut==1)
		{
			floppySpinTimeOut=0;

			floppy_selectDrive(DRIVE_SELECT_NONE);
			floppy_setMotor(0,0);
			mfm_setEnableState(DISABLE);
		}

		/*
		if (diffCollector_Anz==999 && diffCollector_printIt!=999)
		{
			printf("%d\n",diffCollector[diffCollector_printIt]);

			if (diffCollector_printIt<999)
				diffCollector_printIt++;
		}
		*/


		if (usb_recv_len)
		{
			//printf("RX!!!\n");
			usb_recv_len=0;

			if (!strncmp((char*)usb_recv_data,"Floppy",6))
			{
				//printf("Command: %d %d\n",usb_recv_data[6],usb_recv_data[7]);

				if (usb_recv_data[6]==1)
				{
					usb_send_data=usb_blockedGetTxBuf();

					memcpy((char*)usb_send_data,(char*)usb_recv_data,8);
					usb_startTransmit(8);
					usb_recv_len=0;

					int track=usb_recv_data[7];

					printf("Read the track %d!\n",track);

					floppy_selectDrive(DRIVE_SELECT_A);
					floppy_setMotor(0,1);
					mfm_setDecodingMode(MFM_ISO_HD);
					mfm_setEnableState(ENABLE);

					int ret=floppy_readCylinder(track,18);
					//int ret=0;

					if (ret==0)
					{

						unsigned char *ptr=trackBuffer;
						int totalBytesToTransmit=18*512*2;
						int bytesToTransmit;

						crc=0xffff;

						while (totalBytesToTransmit > 0)
						{
							usb_send_data=usb_blockedGetTxBuf();

							if (totalBytesToTransmit > 64)
								bytesToTransmit=64;
							else
								bytesToTransmit=totalBytesToTransmit;

							memcpy(usb_send_data, ptr, bytesToTransmit);
							for (i=0;i<bytesToTransmit;i++)
							{
								crc_shiftByte(usb_send_data[i]);
							}
							usb_startTransmit(bytesToTransmit);

							ptr+=bytesToTransmit;
							totalBytesToTransmit-=bytesToTransmit;
						}

						usb_send_data=usb_blockedGetTxBuf();

						usb_send_data[0]=crc>>8;
						usb_send_data[1]=crc&0xff;

						usb_startTransmit(2);
					}
					else
					{
						usb_send_data=usb_blockedGetTxBuf();
						usb_send_data[0]='E';
						usb_send_data[1]='R';
						usb_send_data[2]='R';
						usb_send_data[3]=ret;
						usb_startTransmit(4);
					}
				}
				else if (usb_recv_data[6]==2)
				{

					floppy_selectDrive(DRIVE_SELECT_A);
					floppy_setMotor(0,1);

					printf("floppy_discoverFloppyFormat\n");
					enum mfmMode fmt=floppy_discoverFloppyFormat();

					usb_send_data=usb_blockedGetTxBuf();
					usb_send_data[0]='F';
					usb_send_data[1]='M';
					usb_send_data[2]='T';
					usb_send_data[3]=fmt;
					usb_startTransmit(4);
				}
				else
				{
					printf("Unknown command!\n");
				}
				floppySpinTimeOut=500;

			}
			else
			{
				printf("Unknown frame!\n");
			}

		}

	}
}

