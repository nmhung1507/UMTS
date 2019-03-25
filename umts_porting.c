/* Includes ------------------------------------------------------------------*/
#include "umts_porting.h"
#ifdef UMTS_CFG_MCU_STM32H7
#include "stm32f7xx_hal.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "..\gsm_mux\GsmMux.h"
#include "..\pppos\pppos.h"

/* Private defines -----------------------------------------------------------*/
#define MAIN_UART_HANDLER  huart6
#if !UMTS_USE_MUX
#define GPS_UART_HANDLER  huart5
#endif

/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static uint8_t rx_byte;
#if !UMTS_USE_MUX
static uint8_t gps_rx_byte = 0;
#endif

/* Global variable -----------------------------------------------------------*/
extern UART_HandleTypeDef huart6;
extern xQueueHandle gMainUartQueue;
extern EventGroupHandle_t 			PPPOS_RxDataEventGroup;
#if UMTS_USE_MUX
extern xQueueHandle				gMuxUartQueue;
#else
extern UART_HandleTypeDef huart5;
extern xQueueHandle gGpsUartQueue;
#endif
extern bool bTransmitting;

/* Private function prototypes------------------------------------------------*/
/* Function implementation ---------------------------------------------------*/
void UMTS_OpenMainUART_LL(void)
{
  HAL_UART_Receive_IT(&MAIN_UART_HANDLER, &rx_byte, 1);
}

#if !UMTS_USE_MUX
void UMTS_OpenGpsUART_LL(void)
{
  HAL_UART_Receive_IT(&GPS_UART_HANDLER, &rx_byte, 1);
}
#endif

/**
 * @brief Callback on UART receive done
 * @param huart
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	BaseType_t xHigherPriorityTaskWoken;
#if UMTS_USE_MUX
  /* Set transmission flag: transfer complete */
  if(huart == &MAIN_UART_HANDLER)
  {
		bool bHigherPriorityTaskWoken = Mux_UARTInputByteFromISR(rx_byte);
    if(xHigherPriorityTaskWoken)
    {
      taskYIELD();
    }
    /* activate UART receive interrupt every time */
    HAL_UART_Receive_IT(&MAIN_UART_HANDLER, (uint8_t*)&rx_byte, 1);  
		
  }
#else
  if(huart == &MAIN_UART_HANDLER)
	{
		xQueueSendToBackFromISR(gMainUartQueue, &rx_byte, &xHigherPriorityTaskWoken);
		
    if(xHigherPriorityTaskWoken)
    {
      taskYIELD();
    }
    /* activate UART receive interrupt every time */
    HAL_UART_Receive_IT(&MAIN_UART_HANDLER, (uint8_t*)&rx_byte, 1);  
	}
  else if(huart == &GPS_UART_HANDLER)
  {
    xQueueSendToBackFromISR(gGpsUartQueue, &gps_rx_byte, &xHigherPriorityTaskWoken);
    if(xHigherPriorityTaskWoken)
    {
      taskYIELD();
    }
    /* activate UART receive interrupt every time */
    HAL_UART_Receive_IT(&GPS_UART_HANDLER, (uint8_t*)&gps_rx_byte, 1);  
  }
#endif

}

bool UMTS_Transmit_LL(uint8_t *pu8Data, int iSize)
{
  return HAL_UART_Transmit_IT(&MAIN_UART_HANDLER, pu8Data, iSize) == HAL_OK ? false : true;
}

/**
 * @brief Callback on UART send done
 * @param huart
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart == &MAIN_UART_HANDLER)
  {
    bTransmitting = false;
  }
}

#else
#error "Port for other MCU here!"
#endif

/*****************************END OF FILE**************************************/
