
#ifndef FLOPPY_MFM_H
#define FLOPPY_MFM_H

#include <stdint.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"

//Bei 168 MHz und einer DD Diskette ist der minimale Abstand einer Transition 672 Takte von TIM2.
//Dies ist der Übergang von einem 1er Bit zum nächsten 1er Bit
//Wir haben alledings nur die Hälfte wegen einem Prescaler.

#define MFM_BITTIME_DD 336 //336
#define MFM_BITTIME_HD (MFM_BITTIME_DD / 2)

#define MAXIMUM_VALUE (MFM_BITTIME_DD * 5)

#define AMIGA_MFM_MASK 0x55555555

#define SYNC_WORD_ISO 0x4489 //broken A1
#define SYNC_WORD_AMIGA 0x44894489ul //broken A1 *2


extern uint32_t flux_decodeCellLength;

extern volatile uint8_t mfm_decodedByte;
extern volatile uint32_t mfm_inSync;
extern volatile uint32_t mfm_decodedByteValid;
extern volatile uint32_t mfm_savedRawWord;


extern uint32_t mfm_lastBit;


unsigned short mfm_iso_encode(unsigned char data);

//void mfm_setDecodingMode(enum fluxMode mode);


void printShortBin(unsigned short val);
void printCharBin(unsigned char val);
void printLongBin(unsigned long val);

void mfm_blockedRead();
void mfm_blockedWaitForSyncWord(int expectNum);

//Wichtig für den Unit Test, um während des Wartens Inputs zu liefern.
#ifdef CUNIT

	void activeWaitCbk(void);

	#define ACTIVE_WAITING activeWaitCbk();
#else
	#define ACTIVE_WAITING
#endif


void mfm_amiga_transitionHandler();
void mfm_iso_transitionHandler();


#endif
