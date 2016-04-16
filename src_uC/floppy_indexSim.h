
#ifndef FLOPPY_INDEXSIM_H
#define FLOPPY_INDEXSIM_H

#include <stdint.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"

void floppy_indexSim_init();
void floppy_indexSim_setEnableState(FunctionalState state);

#endif
