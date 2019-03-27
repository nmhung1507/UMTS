/* Includes ------------------------------------------------------------------*/
#include "gps.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"
#include "umts_serial.h"
#include "umts.h"
#include "string.h"
#include "../gsm_mux/GsmMux.h"

/* Private defines -----------------------------------------------------------*/
#define PERIOD_NUM_DIGIT  9
#define TSK_NAME  "GPS_Task"
#define TSK_STACK_SIZE  192
#define TSK_PARAM_PASSED 1
#define RMC_SIGNAL_NUM  13
#define MTX_TIMEOUT 100
#define DATA_LOCK()  xSemaphoreTake(gpsMutex, MTX_TIMEOUT)
#define DATA_UNLOCK()  xSemaphoreGive(gpsMutex)

/* RMC Signals index table */
const unsigned char rmcSignalIdxTbl[RMC_SIGNAL_NUM] =  
{
  0,  /* Header */
  5,  /* Time */
  13, /* active/void */
  14, /* Latitude info */
  26, /* Latitude tail (N or S)*/
  27, /* Longitude info */
  40, /* Longitude tail (E or W) */
  47, /* Unused */
  47, /* Unused */
  41, /* Date */
  47, /* Unused */
  47, /* Unused */
  47  /* Unused */  
};
/* Private typedef -----------------------------------------------------------*/
typedef enum
{
  PARSE_IDLE,      /* Idling state */
  PARSE_RCVING,    /* Receiving */
  PARSE_WAIT_CS1,  /* Already receive data, waitting for first checksum byte */
  PARSE_WAIT_CS2   /* Waitting for second checksum byte */
}parseStep_t;

typedef struct
{
  char header[5];
  char hour[2];
  char minute[2];
  char second[2];
  char subSec[2];
  char voidOrActive;
  char latDegree[2];
  char latMinute[10];
  char latTail;
  char longDegree[3];
  char longMinute[10];
  char longTail;
  char day[2];
  char month[2];
  char year[2];
  char unused[5];
}RmcRawMsg_t;


/* Private variables ---------------------------------------------------------*/
static QueueHandle_t gpsMutex = NULL;
static TaskHandle_t gpsTskHandle = NULL;
static RmcInfo_t rmcInfo;
static parseStep_t parseStep = PARSE_IDLE;

/* Global variables ----------------------------------------------------------*/
extern xQueueHandle gGpsUartQueue;

/* Private function prototypes------------------------------------------------*/
static void GPS_Task(void  * pvParameters);

/* Convert char to integer, eg: '1' -> 0x01, 'A' -> 0x0A */
static char char2hex(char input);
static void DequeueHandler(char queueVal);
static void UpdateGpsData(RmcRawMsg_t* rmcMessage);

/* Function implementation ---------------------------------------------------*/
/* Convert character to hex, eg: '1' -> 0x01, 'A' -> 0x0A */
static char char2hex(char input)
{
  if((input >= '0') && (input <= '9'))
    return (input - '0');
  else if((input >= 'A') && (input <= 'F'))
    return (input - 0x37); /* Offset 0x37 = 'A' - 0x0A */
  else
    return 0xFF; /* Invalid input */
}

GPS_State_t GPS_Init(void)
{
#if UMTS_USE_MUX
	Mux_DLCSetRecvDataCallback(MUX_CHANNEL_GPS_OUTPUT, Serial_PushDataToGpsQueue);
#endif
  /* End session GPS, don't care result */
  Umts_SendAT(UMTS_CMD_GPS_END, UMTS_OK_STR, UMTS_ERROR_STR, 
    sizeof(UMTS_CMD_GPS_END), UMTS_CMD_QGPSCFG_TIMEOUT, NULL, 0);
  
  /* Config RMC format for GPS */
  if(!Umts_SendAT(UMTS_CMD_GPS_CFG_OUTPUT_RMC, UMTS_OK_STR, NULL, 
    sizeof(UMTS_CMD_GPS_CFG_OUTPUT_RMC), UMTS_CMD_QGPSCFG_TIMEOUT, NULL, 0))
    return GPS_ERROR;
    
  /* Config outport GPS CMUX3 or Debug Uart*/
#if UMTS_USE_MUX
  if(!Umts_SendAT(UMTS_CMD_GPS_CFG_OUTPORT_CMUX3, UMTS_OK_STR, NULL, 
    sizeof(UMTS_CMD_GPS_CFG_OUTPORT_CMUX3)-1, UMTS_CMD_QGPSCFG_TIMEOUT, NULL, 0))
    return GPS_ERROR;
#else
  if(!Umts_SendAT(UMTS_CMD_GPS_CFG_OUTPORT_UARTDEBUG, UMTS_OK_STR, NULL, 
    sizeof(UMTS_CMD_GPS_CFG_OUTPORT_UARTDEBUG), UMTS_CMD_QGPSCFG_TIMEOUT, NULL, 0))
    return GPS_ERROR;
#endif
    
  /* Config period and start gps session */
  char cfgTimeCmd[sizeof(UMTS_CMD_GPS_PERIOD) + PERIOD_NUM_DIGIT] = {0};
  char cPeriod[PERIOD_NUM_DIGIT] = {0};
  strcpy(cfgTimeCmd, UMTS_CMD_GPS_PERIOD);
  /*append period to the end of the command */
  sprintf(cPeriod, "%d", GPS_PERIOD_SEC);
  strcat(cfgTimeCmd, cPeriod);
  /* Append newline characters */
  strcat(cfgTimeCmd, UMTS_NEWLINE_STR);
  /* Send command */
  if(!Umts_SendAT(cfgTimeCmd, UMTS_OK_STR, NULL, 
              sizeof(cfgTimeCmd), UMTS_CMD_QGPS_TIMEOUT, NULL, 0))
    return GPS_ERROR;
											
  /* Create task to receive and process GPS data from gGpsUartQueue */
  if(gpsTskHandle == NULL)
  {
    BaseType_t xReturned;
    xReturned = xTaskCreate(GPS_Task, TSK_NAME, TSK_STACK_SIZE,(void*)TSK_PARAM_PASSED, 
      tskIDLE_PRIORITY, &gpsTskHandle );
    if( xReturned == pdFAIL )
    {
      /* Create task fail, return error */
      return GPS_ERROR;
    }
  }

  /* Create mutex to protect GPS global data */
  if (gpsMutex == NULL)
  {
    gpsMutex = xSemaphoreCreateMutex();
    if(gpsMutex == NULL)
    {
      /* Detele the task created before */
      vTaskDelete(gpsTskHandle);
      gpsTskHandle = NULL;
      return GPS_ERROR;
    }
  }
	
  return GPS_OK;
}

void GPS_Deinit(void)
{
  /* Detele the mutex and task created before */
  if(gpsMutex != NULL)
  {
    vSemaphoreDelete(gpsMutex);
    gpsMutex = NULL;
  }
  if(gpsTskHandle != NULL)
  {
    vTaskDelete(gpsTskHandle);
    gpsTskHandle = NULL;
  }
}

static void GPS_Task(void  * pvParameters)
{
  configASSERT(((uint32_t)pvParameters) == TSK_PARAM_PASSED);
  char   queueVal = 0;
  for(;;)
  {
    /* Loop if exist data in the queue */
    while(uxQueueMessagesWaiting(gGpsUartQueue))
    {
      /* Receive data */
      if(xQueueReceive(gGpsUartQueue, &queueVal, SERIAL_RECV_BYTE_TIMEOUT) != pdPASS)
      {
        break;
      }

      /* Handle data */
      DequeueHandler(queueVal);
    }

    vTaskDelay((3) / portTICK_PERIOD_MS);
  }
}

static void DequeueHandler(char queueVal)
{
  /* Example data: $GPRMC,114903.0,A,2059.909796,N,10552.022035,E,,,180319,,,A*61 */
  static char checkSum = 0;
  static char rmcData[sizeof(RmcRawMsg_t)];
  static short   index = 0, signalCnt = 0;
  printf("%c ", queueVal);
  if(queueVal == '$')
  {
    parseStep = PARSE_RCVING;
    checkSum = 0;
    signalCnt = 0;
    index = rmcSignalIdxTbl[signalCnt];
    memset(rmcData, 0, sizeof(sizeof(RmcRawMsg_t)));
    return;
  }
  else if(queueVal == '*')
  {
    parseStep = PARSE_WAIT_CS1;
    return;
  }
  
  if(parseStep == PARSE_RCVING)
  {
    checkSum ^= queueVal;
    /* If ',' jump index to new position, else fill data to array */
    if(queueVal == ',')
      index = rmcSignalIdxTbl[++signalCnt];
    else
      rmcData[index++] = queueVal;
  }
  /* Received core data and the 1st checksum byte came */
  else if(parseStep == PARSE_WAIT_CS1)
  {
    if((checkSum >> 4) != char2hex(queueVal))
      parseStep = PARSE_IDLE;
    else
      parseStep = PARSE_WAIT_CS2;
  }
  /* 2nd checksum byte came */
  else if(parseStep == PARSE_WAIT_CS2)
  {
    if((checkSum & 0x0F) == char2hex(queueVal))
    {
      RmcRawMsg_t *rmcMessage;
      rmcMessage = (RmcRawMsg_t*)rmcData;
      /* Check if data Active or Void, and header must include "RMC" */
      if((rmcMessage->voidOrActive == 'A') && (rmcMessage->header[2] == 'R') 
        && (rmcMessage->header[3] == 'M') && (rmcMessage->header[4] == 'C'))
      {
        UpdateGpsData(rmcMessage);
      }
    }
    parseStep = PARSE_IDLE;
  }
}

void UpdateGpsData(RmcRawMsg_t* rmcMessage)
{
  unsigned char latLongDeg = 0;
  float latLongMin = 0;
  DATA_LOCK();
  /* Update date to global data */
  rmcInfo.dateTime.day = char2hex(rmcMessage->day[0]) * 10 + char2hex(rmcMessage->day[1]);
  rmcInfo.dateTime.month = char2hex(rmcMessage->month[0]) * 10 + char2hex(rmcMessage->month[1]);
  rmcInfo.dateTime.year = char2hex(rmcMessage->year[0]) * 10 + char2hex(rmcMessage->year[1]);
  
  /* Update time */
  rmcInfo.dateTime.hour = char2hex(rmcMessage->hour[0]) * 10 + char2hex(rmcMessage->hour[1])
    + UMTS_GPS_GMT_OFFSET;
  rmcInfo.dateTime.minute = char2hex(rmcMessage->minute[0]) * 10 + char2hex(rmcMessage->minute[1]);
  rmcInfo.dateTime.second = char2hex(rmcMessage->second[0]) * 10 + char2hex(rmcMessage->second[1]);
  
  /* Update latitude info */
  latLongDeg = char2hex(rmcMessage->latDegree[0]) * 10 + char2hex(rmcMessage->latDegree[1]);
  latLongMin = (float)atof(rmcMessage->latMinute);
  rmcInfo.latitude = latLongDeg + latLongMin / 60;
  rmcInfo.latTail = rmcMessage->latTail;
  
  /* Update longitude */
  latLongDeg = char2hex(rmcMessage->longDegree[0]) * 100 + char2hex(rmcMessage->longDegree[1]) * 10
    + char2hex(rmcMessage->longDegree[2]);
  latLongMin = (float)atof(rmcMessage->longMinute);
  rmcInfo.longitude = latLongDeg + latLongMin / 60;
  rmcInfo.longTail = rmcMessage->longTail;
  
  DATA_UNLOCK();
}

RmcInfo_t GPS_GetData(void)
{
  RmcInfo_t retVal;
  DATA_LOCK();
	/* Copy to clone variables and return */
  memcpy(&retVal, &rmcInfo, sizeof(rmcInfo));
  DATA_UNLOCK();
  return retVal;
}

/*****************************END OF FILE***************************************/
