/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __UMTS_SERIAL_H
#define __UMTS_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stdbool.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define SERIAL_RECV_BYTE_TIMEOUT		50
/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Init serial
  * @param  None
  * @retval None
  */
void Serial_Init(void);

/**
  * @brief  Send Uart
  * @param  Data buffer
  * @param  Data size
  * @retval Result: false:OK, true: error
  */
bool Serial_SendUART(unsigned char* pcData, int iSize);

/**
  * @brief  Receive Uart
  * @param  Data buffer
  * @param  Max length of the buffer
  * @retval length of the data
  */
unsigned short Serial_RecvUART(char* buffer, unsigned short maxLength);

/**
  * @brief  Check if Uart is transmitting or not
  * @param  None
  * @retval true: busy, false: idle
  */
bool Serial_CheckTransmitBusy(void);

/**
  * @brief  Set that uart transmit done
  * @param  None
  * @retval None
  */
void Serial_SetTransmitDone(void);

/* Private defines -----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __UMTS_SERIAL_H */

/******************************END OF FILE*************************************/
