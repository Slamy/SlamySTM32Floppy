/*
 * floppy_gcr_read.h
 *
 *  Created on: 31.07.2016
 *      Author: andre
 */

#ifndef SRC_UC_FLOPPY_GCR_READ_H_
#define SRC_UC_FLOPPY_GCR_READ_H_


extern volatile uint8_t gcr_decodedByte;
extern volatile uint8_t rawGcrSaved;
extern const unsigned char gcrEncodeTable[];
extern const unsigned char gcrDecodeTable[];

void gcr_blockedWaitForSyncState();
void gcr_blockedRead();
void gcr_c64_transitionHandler();
void gcr_blockedReadRawByte();
void gcr_c64_5CellsNoTransitionHandler();



#endif /* SRC_UC_FLOPPY_GCR_READ_H_ */
