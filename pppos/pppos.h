/*
 * pppos_uc20.c
 *
 *  Created on: Mar 18, 2019
 *      Author: phamv_000
 */

#ifndef _LIBGSM_H_
#define _LIBGSM_H_

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "pppos_cfg.h"

#define PPPOS_OK_STR 				"OK"

#define GSM_DATA_RECEIVED_EVENT		0x1U

#define PPPOS_CMD_AT				"AT\r\n"
#define PPPOS_CMD_AT_TIMEOUT		1000
#define PPPOS_CMD_AT_DELAY_NEXT		500

#define PPPOS_CMD_CNMI				"AT+CNMI=0,0,0,0,0\r\n"
#define PPPOS_CMD_CNMI_TIMEOUT		1000
#define PPPOS_CMD_CNMI_DELAY_NEXT	500

#define PPPOS_CMD_RESET				"ATZ\r\n"
#define PPPOS_CMD_RESET_TIMEOUT		500
#define PPPOS_CMD_RESET_DELAY_NEXT	500

#define PPPOS_CMD_RFON				"AT+CFUN=1\r\n"
#define PPPOS_CMD_RFON_TIMEOUT		5000
#define PPPOS_CMD_RFON_DELAY_NEXT	500

#define PPPOS_CMD_RFOFF				"AT+CFUN=4\r\n"
#define PPPOS_CMD_RFOFF_TIMEOUT		5000

#define PPPOS_CMD_GET_CFUN			"AT+CFUN?\r\n"
#define PPPOS_CMD_GET_CFUN_RESP_OK	"+CFUN: 1"
#define PPPOS_CMD_GET_CFUN_TIMEOUT	2000

#define PPPOS_CMD_ATH				"ATH\r\n"
#define PPPOS_CMD_ATH_NO_CARRIER_RESP	"NO CARRIER"
#define PPPOS_CMD_ATH_TIMEOUT		3000

#define PPPOS_CMD_ECHO_OFF			"ATE0\r\n"
#define PPPOS_CMD_ECHO_OFF_TIMEOUT	500
#define PPPOS_CMD_ECHO_OFF_DELAY_NEXT 500

#define PPPOS_CMD_CPIN				"AT+CPIN?\r\n"
#define PPPOS_CMD_PIN_RESP_OK		"CPIN: READY"
#define PPPOS_CMD_CPIN_TIMEOUT		10000
#define PPPOS_CMD_CPIN_DELAY_NEXT	500

#define PPPOS_CMD_CCID				"AT+CCID\r\n"
#define PPPOS_CMD_CCID_TIMEOUT		1000
#define PPPOS_CMD_CCID_DELAY_NEXT	500

#define PPPOS_CMD_CIMI				"AT+CIMI\r\n"
#define PPPOS_CMD_CIMI_TIMEOUT		1000
#define PPPOS_CMD_CIMI_DELAY_NEXT	500

#define PPPOS_CMD_CREG				"AT+CREG?\r\n"
#define PPPOS_CMD_REG_RESP_OK		"CREG: 0,1"
#define PPPOS_CMD_CREG_TIMEOUT		4000
#define PPPOS_CMD_CREG_DELAY_NEXT	100

#define PPPOS_CMD_CONNECT			"AT+CGDATA=\"PPP\",1\r\n"
#define PPPOS_CMD_CONNECT_RESP_OK	"CONNECT"
#define PPPOS_CMD_CONNECT_TIMEOUT	10000
#define PPPOS_CMD_CONNECT_DELAY_NEXT 500

#define PPPOS_CMD_CMGF				"AT+CMGF=1\r\n"
#define PPPOS_CMD_CMGF_TIMEOUT		1000

#define PPPOS_CMD_MAX_FAIL_COUNT	5

#define NULL_CHAR					'\0'
#define SPACE_CHAR					0x20
#define DEL_CHAR					0x7F
#define DOT_CHAR					'.'

#define PPPOS_RESPONSE_BUF_LEN		256
#define PPPOS_CCID_BUF_LEN			25
#define PPPOS_CIMI_BUF_LEN			20
#define PPPOS_BUF_LEN_64			64

#define PPPOS_SEND_MAX_RETRY		100
#define PPPOS_RECV_BYTE_TIMEOUT		50
#define PPPOS_SLEEP_ON_CMD_FAIL		1000
#define PPPOS_SLEEP_10MS			10

#define DISCONNECT_SLEEP_ON_RETRY	1100
#define SWITCH_MODE_STR				"+++"
#define DISCONNECT_SEND_TEST_COUNT	10

typedef enum {
	GSM_STATE_DISCONNECTED = 0,
	GSM_STATE_CONNECTED = 1,
	GSM_STATE_IDLE = 89,
	GSM_STATE_FIRSTINIT = 98
} PPPOS_StateType;

typedef struct 
{
	PPPOS_StateType	State;
	int 		iDoConnect;
	uint32_t 	u32RxCount;
	uint32_t 	u32TxCount;
	bool 		bTaskStarted;
	bool 		bRfOff;
} PPPOS_HandleType;

typedef struct
{
	char		*pcCmd;					/* Command */
	uint16_t	u16CmdSize;				/* Command size */
	char		*pcCmdResponseOnOK;		/* Expected response */
	uint16_t	i16TimeoutMs;			/* Wait for response timeout */
	uint16_t	i16DelayMs;				/* Delay before send next command */
	bool		bSkip;					/* Skip this command */
} PPPOS_CmdType;

typedef void (*PPPOS_ConnectedCallbackType)(void);
typedef void (*PPPOS_ErrorCallbackType)(int iErrCode);

extern struct netif 		PPPOS_gPPPNetIf;
extern PPPOS_HandleType		PPPOS_xHandle;

/*
 * Create GSM/PPPoS task if not already created
 * Initialize GSM and connect to Internet
 * Handle all PPPoS requests
 * Disconnect/Reconnect from/to Internet on user request
 */
//==============
int PPPOS_Init(bool waitUntilConnected);

/*
 * Disconnect from Internet
 * If 'end_task' = 1 also terminate GSM/PPPoS task
 * If 'rfoff' = 1, turns off GSM RF section to preserve power
 * If already disconnected, this function does nothing
 */
//====================================================
void PPPOS_Disconnect(bool bEndTask, bool bRfOff);

/*
 * Get transmitted and received bytes count
 * If 'rst' = 1, resets the counters
 */
//=========================================================
void PPPOS_GetDataCounters(uint32_t *rx, uint32_t *tx, bool bRst);

/*
 * Resets transmitted and received bytes counters
 */
//====================
void PPPOS_ResetDataCounters(void);

/*
 * Get GSM/Task status
 *
 * Result:
 * GSM_STATE_DISCONNECTED	(0)		Disconnected from Internet
 * GSM_STATE_CONNECTED		(1)		Connected to Internet
 * GSM_STATE_IDLE			(89)	Disconnected from Internet, Task idle, waiting for reconnect request
 * GSM_STATE_FIRSTINIT		(98)	Task started, initializing PPPoS
 */
//================
int PPPOS_GetStatus(void);

/*
 * Turn GSM RF Off
 */
//==============
int PPPOS_SetRFOff(void);

/*
 * Turn GSM RF On
 */
//=============
int PPPOS_SetRFOn(void);

// get sim ccid and sim cimi
void PPPOS_GetSIMInfo(char *ccid, char *cimi);

bool PPPOS_PushByteToQueueFromISR(char cRxChar);

void PPPOS_SetConnectedCallback(PPPOS_ConnectedCallbackType pfn);

void PPPOS_SetErrorCallback(PPPOS_ErrorCallbackType pfn);

void 			PPPOS_ShowCmdInfo(const char *pcCmd, int iCmdSize, const char *pcInfo);

int      		PPPOS_ClearRxBuf(void);
void     		PPPOS_RemoveSubStr (char *pcString, char *pcSub) ;
void 			PPPOS_DoDisconnectAT(bool bRfOff);
bool 			PPPOS_DoInitAT(void);
bool 			PPPOS_DoConnectAT(void);
void 			PPPOS_DoGetSimCCID(void);
void 			PPPOS_DoGetSimCIMI(void);
void 	        PPPOS_Lock(void);
void 	        PPPOS_Unlock(void);

bool 			PPPOS_PushDataToQueue(uint8_t *pu8Data, int iSize);

#if PPPOS_CFG_MODEM_UBLOX > 0
void 			PPPOS_DoGetSimCCID_Ublox(char *pBuffer);
void 			PPPOS_DoGetSimCIMI_Ublox(char *pBuffer);
bool 			PPPOS_DoInitAT_Ublox(void);
bool 			PPPOS_DoConnectAT_Ublox(void);
void 			PPPOS_DoDisconnectAT_Ublox(bool bRfOff);
void 			PPPOS_EnableInitCmds_Ublox(void);
void 			PPPOS_EnableConnectCmds_Ublox(void);
int 			PPPOS_SetRFOff_Ublox(void);
int 			PPPOS_SetRFOn_Ublox(void);
#endif

#if PPPOS_CFG_MODEM_UC20 > 0
void 			PPPOS_DoGetSimCCID_UC20(char *pBuffer);
void 			PPPOS_DoGetSimCIMI_UC20(char *pBuffer);
void 			PPPOS_DoGetOperator_UC20(void);
bool 			PPPOS_DoInitAT_UC20(void);
bool 			PPPOS_DoConnectAT_UC20(void);
void 			PPPOS_DoDisconnectAT_UC20(bool bRfOff);
void 			PPPOS_EnableInitCmds_UC20(void);
void 			PPPOS_EnableConnectCmds_UC20(void);
int 			PPPOS_SetRFOff_UC20(void);
int 			PPPOS_SetRFOn_UC20(void);
#endif

#endif
