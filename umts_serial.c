/* Includes ------------------------------------------------------------------*/
#include "umts_serial.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "umts_porting.h"
#include "usart.h"
#include "umts_config.h"

/* Private defines -----------------------------------------------------------*/
#define UMTS_SEND_MAX_RETRY  100

/* Private typedef -----------------------------------------------------------*/
/* Global variables ----------------------------------------------------------*/
xQueueHandle gMainUartQueue;
xQueueHandle gGpsUartQueue;


/* Private variables ---------------------------------------------------------*/
bool bTransmitting = false;

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
  bTransmitting = true;
  return UMTS_Transmit_LL((uint8_t*) pcData, iSize);
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

bool Serial_CheckTransmitBusy(void)
{
  return bTransmitting;
}

/*****************************END OF FILE***************************************/
