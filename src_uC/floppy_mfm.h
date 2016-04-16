
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


enum fluxMode
{
	FLUX_MODE_MFM_ISO,
	FLUX_MODE_MFM_AMIGA,
	FLUX_MODE_GCR_C64
};

enum fluxEncodeMode
{
	FLUX_MFM_ENCODE,
	FLUX_RAW,
	FLUX_MFM_ENCODE_ODD
};

extern enum fluxMode flux_mode;
extern uint32_t mfm_decodeCellLength;

extern volatile uint8_t mfm_decodedByte;
extern volatile uint32_t mfm_inSync;
extern volatile uint32_t mfm_decodedByteValid;
extern volatile uint32_t mfm_savedRawWord;

extern unsigned int mfm_errorHappened;
extern uint32_t mfm_lastBit;


extern int indexOverflowCount;

void TIM2_IRQHandler(void);
unsigned short mfm_iso_encode(unsigned char data);
void flux_read_init();
void flux_write_init();

void flux_read_setEnableState(FunctionalState state);
void flux_write_setEnableState(FunctionalState state);

//void mfm_setDecodingMode(enum fluxMode mode);


void printShortBin(unsigned short val);
void printCharBin(unsigned char val);
void printLongBin(unsigned long val);

void mfm_blockedRead();
void mfm_blockedWaitForSyncWord(int expectNum);

void flux_blockedWrite(uint32_t word);
void flux_configureWrite(enum fluxEncodeMode mode, int wordLen);
void flux_configureWriteCellLength(uint16_t cellLength);


//Wichtig für den Unit Test, um während des Wartens Inputs zu liefern.
#ifdef CUNIT

	void activeWaitCbk(void);

	#define ACTIVE_WAITING activeWaitCbk();
#else
	#define ACTIVE_WAITING
#endif

extern unsigned int diff;


void gcr_blockedWaitForSyncState();
void gcr_blockedRead();
void gcr_c64_transitionHandler();
extern volatile uint8_t gcr_decodedByte;
extern const unsigned char gcrEncodeTable[];
extern const unsigned char gcrDecodeTable[];

//#define ACTIVATE_DIFFCOLLECTOR

#ifdef ACTIVATE_DIFFCOLLECTOR

#define DIFF_COLLECTOR_SIZE 2000

extern volatile unsigned short diffCollector[DIFF_COLLECTOR_SIZE];
extern volatile unsigned int diffCollector_Anz;
extern volatile unsigned int diffCollectorEnabled;

#endif

extern volatile uint16_t flux_write_nextWord_extraPause;
extern volatile uint32_t flux_write_nextWord_len;
extern volatile uint16_t flux_write_nextWord_cellLength;
extern volatile uint16_t flux_write_nextWord_extraPause;


#endif
