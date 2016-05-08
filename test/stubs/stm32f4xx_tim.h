
#ifndef STUB_TIM_H
#define STUB_TIM_H

struct TIM4_stub
{
	int CCMR2;
	int CNT;
};

extern struct TIM4_stub* TIM2;
extern struct TIM4_stub* TIM3;
extern struct TIM4_stub* TIM4;

#define TIM_IT_CC3 0
#define TIM_IT_CC2 0
#define TIM_FLAG_CC1 0
#define TIM_ForcedAction_InActive          ((uint16_t)0x0040)

#define TIM_CCMR2_OC3M 0
#define TIM_OCMode_Active 0


unsigned int TIM_GetCapture3();
void TIM_ClearITPendingBit(struct TIM4_stub *a, int b);
typedef enum {RESET = 0, SET = !RESET} FlagStatus, ITStatus;


typedef struct
{
  unsigned char      RESERVED0;   /*!< Reserved, 0x02                                            */

} TIM_TypeDef;

#endif
