
#ifndef FLOPPY_MFM_H
#define FLOPPY_MFM_H

enum mfmMode
{
	UNKNOWN,
	MFM_ISO_DD,
	MFM_ISO_HD,
	MFM_AMIGA_DD
};

//Bei 168 MHz und einer DD Diskette ist der minimale Abstand einer Transition 672 Takte von TIM2.
//Dies ist der Übergang von einem 1er Bit zum nächsten 1er Bit
//Wir haben alledings nur die Hälfte wegen einem Prescaler.

#define MFM_BITTIME_DD 336 //336
#define MFM_BITTIME_HD (MFM_BITTIME_DD / 2)

//#define LENGTH_MFM_CELL (LENGTH_1_TO_1 / 2)

#define MAXIMUM_VALUE (MFM_BITTIME_DD * 5)

#define AMIGA_MFM_MASK 0x5555

extern volatile unsigned char mfm_decodedByte;
extern volatile unsigned int mfm_inSync;
extern volatile unsigned int mfm_decodedByteValid;
extern volatile unsigned long mfm_savedRawWord;

void TIM2_IRQHandler(void);
unsigned short mfm_encode(unsigned char data);
void mfm_init();

void mfm_setDecodingMode(enum mfmMode mode);
void mfm_setEnableState(FunctionalState state);



#endif
