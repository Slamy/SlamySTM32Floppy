/*
 * floppy_flux.h
 *
 *  Created on: 31.07.2016
 *      Author: andre
 */

#ifndef SRC_UC_FLOPPY_FLUX_H_
#define SRC_UC_FLOPPY_FLUX_H_

enum fluxMode
{
	FLUX_MODE_MFM_ISO,
	FLUX_MODE_MFM_AMIGA,
	FLUX_MODE_GCR_C64
};

extern enum fluxMode flux_mode;

#endif /* SRC_UC_FLOPPY_FLUX_H_ */
