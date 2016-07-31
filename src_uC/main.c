#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "floppy_settings.h"
#include "floppy_sector.h"
#include "floppy_control.h"
#include "floppy_indexSim.h"
#include "floppy_mfm.h"
#include "tm_stm32f4_usb_vcp.h"
#include "assert.h"
#include "floppy_flux_read.h"
#include "floppy_flux_write.h"

volatile int systickCnt=0;
volatile int floppySpinTimeOut=0;
volatile char printVal=0;


void SysTick_Handler(void)
{
	systickCnt++;

	if (floppySpinTimeOut > 1)
		floppySpinTimeOut--;

	printVal=1;

	/*
	if ((systickCnt%10)==0)
	{
		//STM_EVAL_LEDOn(LED3);
		//STM_EVAL_LEDOn(LED4);

		//STM_EVAL_LEDOff(LED5);
		//STM_EVAL_LEDOff(LED6);

		//printf("%ld\n",TIM4->CNT);
	}
	else
	{
		//STM_EVAL_LEDOff(LED3);
		//STM_EVAL_LEDOff(LED4);

		//STM_EVAL_LEDOn(LED5);
		//STM_EVAL_LEDOn(LED6);
	}
	*/

}


void led_init()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	/* GPIOD clock enable */
	STM_EVAL_LEDInit(LED3);
	STM_EVAL_LEDInit(LED4);
	STM_EVAL_LEDInit(LED5);
	STM_EVAL_LEDInit(LED6);

	STM_EVAL_LEDOff(LED3);
	STM_EVAL_LEDOff(LED4);
	STM_EVAL_LEDOff(LED5);
	STM_EVAL_LEDOff(LED6);
}

#ifdef ACTIVATE_DIFFCOLLECTOR
volatile unsigned int diffCollector_printIt=0;
#endif

volatile unsigned char *usb_recv_data;
volatile unsigned char usb_recv_len=0;


int main()
{
	unsigned char *usb_send_data;
	int i;
	SysTick_Config(SystemCoreClock/ 100);
	led_init();
	flux_read_init();
	flux_write_init();
	floppyControl_init();
	floppy_indexSim_init();

	printf("Slamy STM32 Floppy Controller\n");

	/*
	mfm_write_setEnableState(1);

	for(;;)
		;
	*/
	TM_USB_VCP_Init();
	//PT_INIT(&floppy_sectorRead_thread_pt);

	//floppy_selectDrive(DRIVE_SELECT_A);
	//floppy_steppingTest();

	floppy_control_senseDrives();

	//floppy_stepToCylinder(4);
	//floppy_stepToCylinder00();

	//floppyControl_setMotor(0,1);
	//mfm_setBitTime(MFM_BITTIME_HD);
	//transmitTrack(0,18);

	printf("At track 00\n");


	gcr_c64_crossVerifyCodeTables();
	//floppy_indexSim_setEnableState(ENABLE);
	//floppy_selectDensity(DENSITY_DOUBLE);

	//floppy_measureRpm();
	/*
	floppy_configureFormat(FLOPPY_FORMAT_AMIGA_DD,0,0,0);
	floppy_writeAndVerifyCylinder(22);

	floppy_configureFormat(FLOPPY_FORMAT_ISO_HD,0,0,0);
	floppy_writeAndVerifyCylinder(0);
	*/

#if 0
	floppy_selectDrive(DRIVE_SELECT_A);
	floppy_setMotor(0,1);
	floppy_configureFormat(FLOPPY_FORMAT_RAW_GCR,0,0,0);

	uint8_t *cylBufPtr=cylinderBuffer;

	//Header für Raw Daten mit Density Infos
	cylBufPtr[cylinderSize++]=0;
	cylBufPtr[cylinderSize++]=30;
	cylBufPtr[cylinderSize++]=2;

	cylBufPtr[cylinderSize++]=0xff;
	cylBufPtr[cylinderSize++]=0xff;
	cylBufPtr[cylinderSize++]=0b00010001;
	cylBufPtr[cylinderSize++]=0b00001000;
	cylBufPtr[cylinderSize++]=0b01000001;

	cylBufPtr[cylinderSize++]=0b00000100;
	cylBufPtr[cylinderSize++]=0b00001000;
	cylBufPtr[cylinderSize++]=0b00010000;
	cylBufPtr[cylinderSize++]=0b00010000;
	cylBufPtr[cylinderSize++]=0b00010000;

	cylBufPtr[cylinderSize++]=0b00001000;
	cylBufPtr[cylinderSize++]=0b00000100;
	cylBufPtr[cylinderSize++]=0b00000100;
	cylBufPtr[cylinderSize++]=0b00000100;
	cylBufPtr[cylinderSize++]=0b00000100;

	cylBufPtr[cylinderSize++]=0b00000100;
	cylBufPtr[cylinderSize++]=0b00000100;
	cylBufPtr[cylinderSize++]=0b00000100;
	cylBufPtr[cylinderSize++]=0b00000100;
	cylBufPtr[cylinderSize++]=0b00000100;

	cylBufPtr[cylinderSize++]=0b00000100;
	cylBufPtr[cylinderSize++]=0b00010001;
	cylBufPtr[cylinderSize++]=0b00010001;
	cylBufPtr[cylinderSize++]=0b00010001;
	cylBufPtr[cylinderSize++]=0b00010001;

	cylBufPtr[cylinderSize++]=0xff;
	cylBufPtr[cylinderSize++]=0xff;
	cylBufPtr[cylinderSize++]=0xff;
	cylBufPtr[cylinderSize++]=0xff;
	cylBufPtr[cylinderSize++]=0xff;

	//Header für Density Data
	cylBufPtr[cylinderSize++]=0;
	cylBufPtr[cylinderSize++]=8;
	cylBufPtr[cylinderSize++]=4; //markiert variable density data

	//Variable Density Data

	cylBufPtr[cylinderSize++]=0;
	cylBufPtr[cylinderSize++]=0;
	cylBufPtr[cylinderSize++]=244>>8;
	cylBufPtr[cylinderSize++]=244&0xff;

	cylBufPtr[cylinderSize++]=0xff;
	cylBufPtr[cylinderSize++]=0xff;
	cylBufPtr[cylinderSize++]=0;
	cylBufPtr[cylinderSize++]=0;

	//Kennzeichne das Ende
	cylBufPtr[cylinderSize++]=0;
	cylBufPtr[cylinderSize++]=0;
	cylBufPtr[cylinderSize++]=0;

	geometry_sectors=1; //bei raw nehmen wir an, es gibt einen großen sector

	floppy_writeAndVerifyCylinder(0);
#endif

	/*
	floppy_debugTrackDataMachine(0,0);
	floppy_debugTrackDataMachine(1,0);
	floppy_debugTrackDataMachine(2,0);
	floppy_debugTrackDataMachine(3,0);
	floppy_debugTrackDataMachine(4,0);
	*/
	/*
	floppy_debugTrackDataMachine(0,1);
	floppy_debugTrackDataMachine(1,0);
	floppy_debugTrackDataMachine(1,1);
	*/

	/*
	for (i=0;i<30;i++)
	{
		floppy_discoverFloppyFormat(i,1);
	}
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
			floppy_setMotorSelectedDrive(0);
			flux_read_setEnableState(DISABLE);
			flux_write_setEnableState(DISABLE);
		}

#ifdef ACTIVATE_DIFFCOLLECTOR
		if (diffCollector_Anz==(DIFF_COLLECTOR_SIZE) && diffCollector_printIt<(DIFF_COLLECTOR_SIZE-1))
		{
			printf("%d ",diffCollector[diffCollector_printIt]);


			while (diffCollector[diffCollector_printIt] > mfm_decodeCellLength + mfm_decodeCellLength/2) //+mfm_cellLength/2 ist die Toleranz die genau auf die Mitte gesetzt wird.
			{
				printf("0");
				diffCollector[diffCollector_printIt]-=mfm_decodeCellLength;
			}
			printf("1\n");

			if (diffCollector_printIt<(DIFF_COLLECTOR_SIZE-1))
				diffCollector_printIt++;
		}
#endif


		usb_recv_len=usb_getRecvLen();
		if (usb_recv_len)
		{
			usb_recv_data=usb_getRecvBuffer();
			//printf("RX!!!\n");


			if (!strncmp((char*)usb_recv_data,"Floppy",6))
			{
				//printf("Command: %d %d\n",usb_recv_data[6],usb_recv_data[7]);

#if 0 //Lesen erstmal deaktiviert
				if (usb_recv_data[6]==1 && usb_recv_len==8) //read cylinder
				{
					usb_send_data=usb_blockedGetTxBuf();

					memcpy((char*)usb_send_data,(char*)usb_recv_data,8);
					usb_startTransmit(8);

					int cylinder=usb_recv_data[7];

					printf("Read the cylinder %d!\n",cylinder);

					floppy_selectDrive(DRIVE_SELECT_A);
					floppy_setMotor(0,1);

					int ret=floppy_readCylinder(cylinder);
					//int ret=0;

					if (ret==0)
					{

						unsigned char *trkBufPtr=(uint8_t*)cylinderBuffer;
						int totalBytesToTransmit=geometry_sectors * geometry_payloadBytesPerSector * geometry_heads;

						//printf("  sending %d byte\n",totalBytesToTransmit);
						int bytesToTransmit;

						crc=0xffff;
						while (totalBytesToTransmit > 0)
						{
							usb_send_data=usb_blockedGetTxBuf();

							if (totalBytesToTransmit > 64)
								bytesToTransmit=64;
							else
								bytesToTransmit=totalBytesToTransmit;

							memcpy(usb_send_data, trkBufPtr, bytesToTransmit);
							for (i=0;i<bytesToTransmit;i++)
							{
								crc_shiftByte(usb_send_data[i]);
							}
							usb_startTransmit(bytesToTransmit);

							trkBufPtr+=bytesToTransmit;
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
#endif
				if (usb_recv_data[6]==2 && usb_recv_len==9) //discover Format
				{
					floppy_selectFittingDrive();
					floppy_setMotorSelectedDrive(1);

					printf("floppy_discoverFloppyFormat\n");
					enum floppyFormat fmt=floppy_discoverFloppyFormat(usb_recv_data[7],usb_recv_data[8]);

					usb_send_data=usb_blockedGetTxBuf();
					usb_send_data[0]='F';
					usb_send_data[1]='M';
					usb_send_data[2]='T';
					usb_send_data[3]=fmt;
					usb_send_data[4]=sectorsDetected;
					printf("fmt id %d\n",fmt);
					usb_startTransmit(5);
				}
				else if (usb_recv_data[6]==3 && usb_recv_len==11) //configure
				{

					floppy_configureFormat(usb_recv_data[7],usb_recv_data[8],usb_recv_data[9],usb_recv_data[10]);


					printf("configuration_flags:%lx\n",configuration_flags);
					usb_send_data=usb_blockedGetTxBuf();
					usb_send_data[0]='O';
					usb_send_data[1]='K';
					usb_startTransmit(2);
				}
				else if (usb_recv_data[6]==4 && usb_recv_len==13) //write cylinder
				{
					int cylinder=usb_recv_data[7];
					geometry_sectors=usb_recv_data[8];
					cylinderSize=(((int)usb_recv_data[9]<<8) | usb_recv_data[10]);

					//geometry_iso_gap3_postID=usb_recv_data[11]; //Das Übernehmen der Gap3 scheint extrem zu stören.

					geometry_iso_fillerByte=usb_recv_data[12];

					int totalBytesToReceive=cylinderSize + 2 ;//2 CRC Bytes

					//memcpy(geometry_iso_sectorPos,(void*)&usb_recv_data[12],MAX_SECTORS_PER_TRACK);

					//printf("Debug %d %d %d\n",geometry_iso_sectorPos[8],geometry_iso_sectorPos[9],geometry_iso_sectorPos[10]);
					unsigned char *trkBufPtr=(uint8_t*)cylinderBuffer;

					//int totalBytesToReceive=geometry_sectors * geometry_heads * geometry_payloadBytesPerSector + 2; //2 CRC Bytes

					printf("Write cyl %d with %d sectors ... waiting for %d byte \n",(int)cylinder,(int)geometry_sectors,totalBytesToReceive);

					crc=0xffff;
					usb_releaseRecvBuffer();
					assert(totalBytesToReceive < sizeof(cylinderBuffer));

					assert (trkBufPtr >= (unsigned char*)&cylinderBuffer[0]);
					assert (trkBufPtr < (unsigned char*)&cylinderBuffer[CYLINDER_BUFFER_SIZE]);

					while (totalBytesToReceive > 0)
					{
						setupStepTimer(50000);
						usb_recv_len=0;
						while (!usb_recv_len)
						{
							usb_recv_len=usb_getRecvLen();
							if (TIM_GetFlagStatus(TIM3,TIM_FLAG_CC1)==SET)
							{
								printf("USB TimeOut\n");
								break;
							}
						}

						usb_recv_data=usb_getRecvBuffer();
						for (i=0;i<usb_recv_len;i++)
						{
							crc_shiftByte(usb_recv_data[i]);
						}
						memcpy(trkBufPtr,(void*)usb_recv_data,usb_recv_len);
						totalBytesToReceive-=usb_recv_len;
						trkBufPtr+=usb_recv_len;
						usb_releaseRecvBuffer();
					}

					assert (trkBufPtr >= (unsigned char*)&cylinderBuffer[0]);
					assert (trkBufPtr < (unsigned char*)&cylinderBuffer[CYLINDER_BUFFER_SIZE]);

					//Die CRC muss nun stimmen, also 0 sein, da sie Teil der Daten war.

					int ret;

					if (!crc)
					{
						/*
						unsigned char *trkBufPtr=(uint8_t*)trackBuffer;

						for (i=0;i<geometry_sectors*geometry_heads;i++)
						{
							printf("Recv Sec %d %x\n",i,trkBufPtr[i*geometry_payloadBytesPerSector]);
						}
						*/

						floppy_selectFittingDrive();
						floppy_setMotorSelectedDrive(1);

						ret=floppy_writeAndVerifyCylinder(cylinder);
						if (ret)
						{
							printf("floppy_writeAndVerifyCylinder failed\n");
						}
						/*
						ret=floppy_writeAndVerifyCylinder(cylinder+1);
						if (ret)
						{
							printf("floppy_writeAndVerifyCylinder failed\n");
						}
						*/
					}
					else
					{
						printf("Received CRC was wrong!\n");
						ret=1;
					}

					usb_send_data=usb_blockedGetTxBuf();
					usb_send_data[0]='W';
					usb_send_data[1]='C';
					usb_send_data[2]='R';
					usb_send_data[3]=ret;
					usb_startTransmit(4);
				}
				else if (usb_recv_data[6]==5 && usb_recv_len==8) //polarize cylinder
				{
					int cylinder=usb_recv_data[7];

					printf("Polarize the cylinder %d!\n",cylinder);

					floppy_selectFittingDrive();
					floppy_setMotorSelectedDrive(1);

					int ret=floppy_polarizeCylinder(cylinder);
					if (ret)
					{
						printf("floppy_polarizeCylinder failed\n");
					}

					usb_send_data=usb_blockedGetTxBuf();
					usb_send_data[0]='P';
					usb_send_data[1]='O';
					usb_send_data[2]='L';
					usb_send_data[3]=ret;
					usb_startTransmit(4);

				}
				else if (usb_recv_data[6]==6 && usb_recv_len==7) //measure rpm
				{
					floppy_selectFittingDrive();
					floppy_setMotorSelectedDrive(1);

					floppy_measureRpm();

					usb_send_data=usb_blockedGetTxBuf();
					usb_send_data[0]='O';
					usb_send_data[1]='K';
					usb_startTransmit(2);
				}
				else
				{
					printf("Unknown command!\n");
				}
				floppySpinTimeOut=250;

			}
			else
			{
				printf("Unknown frame!\n");
			}

			//printf("Standby\n");
			usb_releaseRecvBuffer();

		}

	}
}

