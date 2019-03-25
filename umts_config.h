/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _UMTS_CONFIG_H_
#define _UMTS_CONFIG_H_

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#if UMTS_CFG_MCU_STM32H7
#include "stm32f7xx_hal.h"
#endif
#if UMTS_CFG_DEBUG_EN
#include "print_debug.h"
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define UMTS_CFG_MCU_STM32H7
#define UMTS_USE_MUX  0
#define UMTS_GPS_GMT_OFFSET  7
#define UMTS_CFG_RX_QUEUE_SIZE  128
#define UMTS_CFG_MTX_TIMEOUT 	1000

#define UMTS_MAIN_UART_QUEUE_SIZE 256

#define UMTS_GPS_UART_QUEUE_SIZE  256


/* Config debug */
#define UMTS_CFG_DEBUG_EN   0
#if UMTS_CFG_DEBUG_EN
#define UMTS_DBG  PrintDebug
#else
#define UMTS_DBG(...)
#endif
/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

#endif /* _UMTS_CONFIG_H_ */

/******************************END OF FILE*************************************/
