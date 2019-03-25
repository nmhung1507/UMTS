/*
 * GsmMux_Cfg.h
 *
 *  Created on: Mar 19, 2019
 *      Author: v.daipv1@vinfast.vn
 */

#ifndef _GSMMUX_CFG_H_
#define _GSMMUX_CFG_H_

#include "cmsis_os.h"

/* Configure debug */
#define GSMMUX_CFG_DEBUG		1

#if GSMMUX_CFG_DEBUG > 0
#define GSMMUX_DBG				printf
#else
#define GSMMUX_DBG(...)
#endif

/* Sleep function */
#define GSMMUX_SLEEP(ms)		osDelay(ms)

/* Mux buffer size */
#define GSMMUX_CFG_BUFF_SIZE	256

/* Configure statistic */
#define GSMMUX_CFG_STAT			0

#endif /* _GSMMUX_CFG_H_ */
