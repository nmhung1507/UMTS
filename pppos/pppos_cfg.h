#ifndef _LIBGSM_CFG_H_
#define _LIBGSM_CFG_H_

#include <stdbool.h>
#include "stm32f7xx_hal.h"
#include "umts_config.h"

/* Configure modem type */
#define PPPOS_CFG_MODEM_UBLOX			0
#define PPPOS_CFG_MODEM_UC20			1

#if UMTS_USE_MUX

#include "GsmMux.h"

/* Configure DLC for PPP data */
#define PPPOS_CFG_UC20_DLCI				2
#endif


/* Mutex timeout */
#define PPPOS_CFG_MTX_TIMEOUT 		    (1000)

/* Config enable RX & TX counters */
#define PPPOS_CFG_COUNTERS_EN      		0

/* Config debug */
#define PPPOS_CFG_DEBUG_EN         		1       /* Enable debug */

#if (PPPOS_CFG_DEBUG_EN == 1)

#define PPPOS_DBG                 		printf
#else 
#define PPPOS_DBG(...)
#endif  /*LIBGSM_CFG_DEBUG_EN*/

/* Sleep function */
#define PPPOS_SLEEP(ms)                	vTaskDelay((ms) / portTICK_PERIOD_MS)

/* Mutex lock & unlock */
#define PPPOS_LOCK(mtx, timeout)       	xSemaphoreTake(mtx, timeout)
#define PPPOS_UNLOCK(mtx)              	xSemaphoreGive(mtx)

/* User callbacks */
void PPPOS_ConnectedAppCallback(void);
void PPPOS_ErrorAppCallback(int iErrCode);

#endif
