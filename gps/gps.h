/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FILE_H
#define __FILE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "umts_porting.h"
/* Exported types ------------------------------------------------------------*/
typedef enum
{
  GPS_OK,
  GPS_ERROR
}GPS_State_t;

typedef struct
{
  unsigned char hour;
  unsigned char minute;
  unsigned char second;
  unsigned char day;
  unsigned char month;
  unsigned char year;
}GPS_DateTime_t;

typedef struct
{
  GPS_DateTime_t dateTime;
  float latitude;
  char latTail;
  float longitude;
  char longTail;
}RmcInfo_t;
/* Exported constants --------------------------------------------------------*/
#define GPS_PERIOD_SEC  2
/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  GPS Init
  * @param  None
  * @retval None
  */
GPS_State_t GPS_Init(void);
/**
  * @brief  Get GPS data
  * @param  None
  * @retval GPS data struct
  */
RmcInfo_t GPS_GetData(void);

/* Private defines -----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __FILE_H */

/******************************END OF FILE*************************************/
