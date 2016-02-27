
#ifndef STUB_TIM_H
#define STUB_TIM_H

#define TIM2 0
#define TIM3 0
#define TIM4 0

#define TIM_IT_CC3 0
#define TIM_FLAG_CC1 0
#define TIM_ForcedAction_InActive          ((uint16_t)0x0040)

unsigned int TIM_GetCapture3();
void TIM_ClearITPendingBit(int a, int b);
typedef enum {RESET = 0, SET = !RESET} FlagStatus, ITStatus;


typedef struct
{
  unsigned char      RESERVED0;   /*!< Reserved, 0x02                                            */

} TIM_TypeDef;

#endif
