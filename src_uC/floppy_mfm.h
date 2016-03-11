
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


enum mfmMode
{
	MFM_MODE_ISO,
	MFM_MODE_AMIGA
};

enum mfmEncodeMode
{
	MFM_ENCODE,
	MFM_RAW,
	MFM_ENCODE_ODD
};

extern enum mfmMode mfm_mode;
extern uint32_t mfm_decodeCellLength;

extern volatile uint8_t mfm_decodedByte;
extern volatile uint32_t mfm_inSync;
extern volatile uint32_t mfm_decodedByteValid;
extern volatile uint32_t mfm_savedRawWord;

extern unsigned int mfm_errorHappened;
extern uint32_t mfm_lastBit;


void TIM2_IRQHandler(void);
unsigned short mfm_iso_encode(unsigned char data);
void mfm_read_init();
void mfm_write_init();

void mfm_read_setEnableState(FunctionalState state);
void mfm_write_setEnableState(FunctionalState state);

void mfm_setDecodingMode(enum mfmMode mode);



void printShortBin(unsigned short val);
void printCharBin(unsigned char val);
void printLongBin(unsigned long val);

void mfm_blockedRead();
void mfm_blockedWaitForSyncWord(int expectNum);

void mfm_blockedWrite(uint32_t word);
void mfm_configureWrite(enum mfmEncodeMode mode, int wordLen);
void mfm_configureWriteCellLength(uint16_t cellLength);

#endif
