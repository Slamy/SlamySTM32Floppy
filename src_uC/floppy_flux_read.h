/*
 * floppy_flux_read.h
 *
 *  Created on: 31.07.2016
 *      Author: andre
 */

#ifndef SRC_UC_FLOPPY_FLUX_READ_H_
#define SRC_UC_FLOPPY_FLUX_READ_H_

extern unsigned int floppy_readErrorHappened;


void flux_read_init();
void flux_read_setEnableState(FunctionalState state);

extern unsigned int diff;
extern volatile unsigned int fluxReadCount;

//#define ACTIVATE_DIFFCOLLECTOR

#ifdef ACTIVATE_DIFFCOLLECTOR

#define DIFF_COLLECTOR_SIZE 200

extern volatile unsigned short diffCollector[DIFF_COLLECTOR_SIZE];
extern volatile unsigned int diffCollector_Anz;
extern volatile unsigned int diffCollectorEnabled;

#endif


#define ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO

#ifdef ACTIVATE_DEBUG_RECEIVE_DIFF_FIFO

#define DEBUG_DIFF_FIFO_SIZE 200
extern volatile unsigned int fluxReadDebugFifoValue[DEBUG_DIFF_FIFO_SIZE];
extern volatile unsigned int fluxReadDebugFifo_writePos;
extern volatile unsigned int fluxReadDebugFifo_enabled;

static inline void flux_read_diffDebugFifoWrite(unsigned int val)
{
	if (fluxReadDebugFifo_enabled)
	{
		fluxReadDebugFifoValue[fluxReadDebugFifo_writePos++]=val;
		if (fluxReadDebugFifo_writePos>=DEBUG_DIFF_FIFO_SIZE)
			fluxReadDebugFifo_writePos=0;
	}
}
void printDebugDiffFifo();

#endif


#define FLUX_READ_FIFO_SIZE_MASK 0b0111
#define FLUX_READ_FIFO_SIZE (FLUX_READ_FIFO_SIZE_MASK)+1

extern volatile unsigned short fluxReadFifo[FLUX_READ_FIFO_SIZE];
extern volatile unsigned int fluxReadFifo_writePos;
extern volatile unsigned int fluxReadFifo_readPos;


#define FLUX_DIFF_5_CELLS_WITHOUT_TRANS 0xffff

#endif /* SRC_UC_FLOPPY_FLUX_READ_H_ */
