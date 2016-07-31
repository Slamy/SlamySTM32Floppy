/*
 * floppy_flux_write.h
 *
 *  Created on: 31.07.2016
 *      Author: andre
 */

#ifndef SRC_UC_FLOPPY_FLUX_WRITE_H_
#define SRC_UC_FLOPPY_FLUX_WRITE_H_

enum fluxEncodeMode
{
	FLUX_MFM_ENCODE,
	FLUX_RAW,
	FLUX_MFM_ENCODE_ODD
};

void flux_write_init();
void flux_write_setEnableState(FunctionalState state);
void flux_blockedWrite(uint32_t word);
void flux_configureWrite(enum fluxEncodeMode mode, int wordLen);
void flux_configureWriteCellLength(uint16_t cellLength);
void flux_write_waitForUnderflow();

extern int indexOverflowCount;


//#define ACTIVATE_DEBUG_FLUX_WRITE_FIFO

#ifdef ACTIVATE_DEBUG_FLUX_WRITE_FIFO

#define DEBUG_DIFF_FIFO_SIZE 100
extern volatile unsigned int fluxWriteDebugFifoValue[DEBUG_DIFF_FIFO_SIZE];
extern volatile unsigned int fluxWriteDebugFifo_writePos;
extern volatile unsigned int fluxWriteDebugFifo_enabled;

static inline void flux_write_diffDebugFifoWrite(unsigned int val)
{
	if (fluxWriteDebugFifo_enabled)
	{
		fluxWriteDebugFifoValue[fluxWriteDebugFifo_writePos++]=val;
		if (fluxWriteDebugFifo_writePos>=DEBUG_DIFF_FIFO_SIZE)
			fluxWriteDebugFifo_writePos=0;
	}
}

#endif



#endif /* SRC_UC_FLOPPY_FLUX_WRITE_H_ */
