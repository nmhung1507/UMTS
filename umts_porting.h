/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __UMTS_PORTING_H
#define __UMTS_PORTING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "umts_config.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
#ifdef UMTS_CFG_MCU_STM32H7
/**
  * @brief  Open main Uart, actually start receive interrupt
  * @param  none
  * @retval none
  */
void UMTS_OpenMainUART_LL(void);
/**
  * @brief  Main Uart low level UART transmit data
  * @param  data buffer
  * @param  size of data
  * @retval result: false:OK, true: error
  */
bool UMTS_Transmit_LL(unsigned char *pu8Data, int iSize);

#if !UMTS_USE_MUX
/**
  * @brief  Open main Uart, actually start receive interrupt
  * @param  none
  * @retval none
  */
void UMTS_OpenGpsUART_LL(void);
#endif /* UMTS_USE_MUX */
#endif /* UMTS_CFG_MCU_STM32H7 */
/* Private defines -----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#else
/* Port other MCU here */
#endif /* __UMTS_PORTING_H */

/******************************END OF FILE*************************************/
