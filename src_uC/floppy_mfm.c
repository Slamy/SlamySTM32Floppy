#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "stm32f4_discovery.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "floppy_mfm.h"
#include "floppy_control.h"


uint32_t mfm_decodeCellLength=MFM_BITTIME_DD/2;

enum fluxMode flux_mode;
uint32_t mfm_sectorsPerTrack=0;





