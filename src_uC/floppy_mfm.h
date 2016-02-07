
#ifndef FLOPPY_MFM_H
#define FLOPPY_MFM_H

#include "pt.h"
#include "pt-sem.h"


//Bei 168 MHz und einer DD Diskette ist der minimale Abstand einer Transition 672 Takte von TIM2.
//Dies ist der Übergang von einem 1er Bit zum nächsten 1er Bit

#define MFM_BITTIME_DD 336 //336
#define MFM_BITTIME_HD (MFM_BITTIME_DD / 2)

//#define LENGTH_MFM_CELL (LENGTH_1_TO_1 / 2)

#define MAXIMUM_VALUE (MFM_BITTIME_DD * 5)

enum mfm_decodingStatus_enum
{
	UNSYNC,
	SYNC1,
	SYNC2,
	SYNC3,
	DATA_VALID
};

extern volatile unsigned char mfm_decodedByte;
extern volatile enum mfm_decodingStatus_enum mfm_decodingStatus;

void TIM2_IRQHandler(void);
unsigned short mfm_encode(unsigned char data);
void mfm_init();
void mfm_setBitTime(unsigned int bit);

#endif
