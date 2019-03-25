/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "umts.h"
#include "umts_serial.h"
#include "umts_porting.h"

/* Private defines -----------------------------------------------------------*/
#define UMTS_SLEEP(ms)  vTaskDelay((ms) / portTICK_PERIOD_MS)

/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Global variable -----------------------------------------------------------*/
/* Private function prototypes------------------------------------------------*/
/* Function implementation ---------------------------------------------------*/
int Umts_SendAT(const char * pcCmd, const char *pcResp, const char * pcResp2, 
                      int iCmdSize, int iTimeout, char *pcResponse, int iSize)
{
  char arrcResp[UMTS_RESPONSE_BUF_LEN] = {NULL_CHAR};
  char arrcData[UMTS_RESPONSE_BUF_LEN] = {NULL_CHAR};
  int iLen = 0, iRes = 1, idx = 0, iTot = 0, iTimeoutCnt = 0, iResLen = 0;

  if (pcCmd != NULL)
  {
    if (iCmdSize == -1)
    {
      iCmdSize = strlen(pcCmd);
    }
    UMTS_OpenMainUART_LL();
    Serial_SendUART((unsigned char*)pcCmd, iCmdSize);
    UMTS_SLEEP(1);
  }

  /* Wait for and check the response */
  idx = 0;
  while(1)
  {
    memset(arrcData, 0, UMTS_RESPONSE_BUF_LEN);
    iLen = Serial_RecvUART(arrcData, UMTS_RESPONSE_BUF_LEN);
    /* If received some data -> append to buffer */
    if (iLen > 0)
    {
      for (int i = 0; i < iLen; i++)
      {
        if (idx < UMTS_RESPONSE_BUF_LEN)
        {
          if (((uint8_t)arrcData[i] >= SPACE_CHAR) && ((uint8_t)arrcData[i] < DEL_CHAR))
          {
            arrcResp[idx++] = arrcData[i];
          }
          else
          {
            /* Use '.' instead of invalid characters */
            arrcResp[idx++] = DOT_CHAR;  
          }
        }
      }
      iTot += iLen;
    }
    /* If not received data, check for previous received data */
    else if (iTot > 0)
    {
      /* Check for the 1st expected response */
      if (strstr(arrcResp, pcResp) != NULL)
      {
				if(pcResponse != NULL)
				{
				  memcpy(pcResponse, arrcResp, iSize);
				  iResLen = strlen(pcResponse);
				  if (iResLen > iSize)
				  {
				  	iResLen = iSize;
				  }
				  pcResponse[iResLen] = NULL_CHAR;
			  }
        UMTS_DBG("[PPPOS] AT RESPONSE: [%s]\n", arrcResp);
        break;
      }
      /* Check for the 2nd expected response */
      else if (pcResp2 != NULL)
      {
        if (strstr(arrcResp, pcResp2) != NULL)
        {
          UMTS_DBG("[PPPOS] AT RESPONSE (1): [%s]\n", arrcResp);
          iRes = 2;
          break;
        }
      }
    }

    /* Retry */
    UMTS_SLEEP(1);
    iTimeoutCnt += 1;
    if (iTimeoutCnt > iTimeout)
    {
      /* timeout */
      UMTS_DBG("[PPPOS] AT: TIMEOUT\n");
      iRes = 0;
      break;
    }
  }
  return iRes;
}

/*****************************END OF FILE**************************************/
