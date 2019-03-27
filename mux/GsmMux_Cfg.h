/*
 * GsmMux_Cfg.h
 *
 *  Created on: Mar 19, 2019
 *      Author: v.daipv1@vinfast.vn
 */

#ifndef CHARGEPOINT_V2_INC_CFG_GSMMUX_CFG_H_
#define CHARGEPOINT_V2_INC_CFG_GSMMUX_CFG_H_

#include "cmsis_os.h"

/* Configure debug */
#define GSMMUX_CFG_DEBUG		1

#if GSMMUX_CFG_DEBUG > 0
//#include "print_debug.h"
#define GSMMUX_DBG				printf//PrintDebug
#else
#define GSMMUX_DBG(...)
#endif

/* Sleep function */
#define GSMMUX_SLEEP(ms)		osDelay(ms)

/* Mux buffer size */
#define GSMMUX_CFG_BUFF_SIZE	256

/* UART handle */
#include "stm32f7xx_hal.h"

extern 	UART_HandleTypeDef		huart6;
#define GSMMUX_UART_HANDLE		huart6

/* Debug UART */
extern UART_HandleTypeDef		huart5;

/* Configure statistic */
#define GSMMUX_CFG_STAT			0

#endif /* CHARGEPOINT_V2_INC_CFG_GSMMUX_CFG_H_ */
