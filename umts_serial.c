/* Includes ------------------------------------------------------------------*/
#include "umts_serial.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "umts_porting.h"
#include "usart.h"
#include "umts_config.h"
#include "..\gsm_mux\GsmMux.h"

/* Private defines -----------------------------------------------------------*/
#define UMTS_SEND_MAX_RETRY  100
#if UMTS_USE_MUX
#define SEND_FRAME_MAX_SIZE		512
#endif
/* Private typedef -----------------------------------------------------------*/
/* Global variables ----------------------------------------------------------*/
xQueueHandle gMainUartQueue;
xQueueHandle gGpsUartQueue;
bool bTransmitting = false;

/* Private variables ---------------------------------------------------------*/
#if UMTS_USE_MUX
static uint8_t pu8Frame[SEND_FRAME_MAX_SIZE];
#endif

/* Private function prototypes------------------------------------------------*/
/* Function implementation ---------------------------------------------------*/
void Serial_Init(void)
{
  gMainUartQueue = xQueueCreate(UMTS_MAIN_UART_QUEUE_SIZE, sizeof(unsigned char));
  UMTS_OpenMainUART_LL();
  gGpsUartQueue = xQueueCreate(UMTS_GPS_UART_QUEUE_SIZE, sizeof(unsigned char));
  #if !UMTS_USE_MUX
  UMTS_OpenGpsUART_LL();
  #endif
}

bool Serial_SendUART(unsigned char* pcData, int iSize)
{
  int iTryCount = 0;

  while(bTransmitting)
  {
    vTaskDelay((1) / portTICK_PERIOD_MS); /* Sleep 1ms */
    if(++iTryCount >= UMTS_SEND_MAX_RETRY)
    {
      return false;
    }
  }
  
	#if UMTS_USE_MUX
	return Mux_DLCSendData(MUX_CHANNEL_AT_CMD, pcData, iSize, pu8Frame, SEND_FRAME_MAX_SIZE);
	#else
	bTransmitting = true;
  return UMTS_Transmit_LL((uint8_t*) pcData, iSize);
	#endif
}

unsigned short Serial_RecvUART(char* buffer, uint16_t maxLength)
{
  uint8_t   u8Tmp = 0;
  uint16_t  length = 0;

  while(uxQueueMessagesWaiting(gMainUartQueue))
  {
    if(xQueueReceive(gMainUartQueue, &u8Tmp, SERIAL_RECV_BYTE_TIMEOUT) != pdPASS)
    {
      break;
    }
   
    buffer[length] = u8Tmp;
    length++;

    if (length >= maxLength)
    {
      break;
    }
  }
  
  return length;
}

bool Serial_PushDataToMainQueue(uint8_t *pu8Data, int iSize)
{
	bool bRet = true;
	for(int idx = 0; idx < iSize; idx++)
  {
		if(xQueueSendToBack(gMainUartQueue, &pu8Data[idx], 0) != pdTRUE)
		{
			bRet = false;
			break;
		}
	}

	return bRet;
}

bool Serial_PushDataToGpsQueue(uint8_t *pu8Data, int iSize)
{
	bool bRet = true;

	for(int idx = 0; idx < iSize; idx++)
  {
		if(xQueueSendToBack(gGpsUartQueue, &pu8Data[idx], 0) != pdTRUE)
		{
			bRet = false;
			break;
		}
	}

	return bRet;
}

bool Serial_CheckTransmitBusy(void)
{
  return bTransmitting;
}

/*****************************END OF FILE***************************************/
