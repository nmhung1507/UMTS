/*
 * pppos_uc20.c
 *
 *  Created on: Mar 18, 2019
 *      Author: v.daipv1@vinfast.vn
 */

#include <string.h>
#include "pppos.h"
#include "pppos_cfg.h"
#include "FreeRTOS.h"
#include "task.h"
#include "umts.h"
#include "umts_serial.h"

#if PPPOS_CFG_MODEM_UC20 > 0

static PPPOS_CmdType UC20_xCmdAT =
{
		.pcCmd = "AT\r\n",
		.u16CmdSize = sizeof("AT\r\n")-1,
		.pcCmdResponseOnOK = "OK",
		.i16TimeoutMs = 500,
		.i16DelayMs = 500,
		.bSkip = false
};

static PPPOS_CmdType UC20_xCmdATV1 =
{
		.pcCmd = "ATV1\r\n",
		.u16CmdSize = sizeof("ATV1\r\n")-1,
		.pcCmdResponseOnOK = "OK",
		.i16TimeoutMs = 500,
		.i16DelayMs = 500,
		.bSkip = false
};

static PPPOS_CmdType UC20_xCmdRFOn =
{
		.pcCmd = PPPOS_CMD_RFON,
		.u16CmdSize = sizeof(PPPOS_CMD_RFON) - 1,
		.pcCmdResponseOnOK = PPPOS_OK_STR,
		.i16TimeoutMs = PPPOS_CMD_RFON_TIMEOUT,
		.i16DelayMs = PPPOS_CMD_RFON_DELAY_NEXT,
		.bSkip = false,
};

static PPPOS_CmdType UC20_xCmdEchoOff =
{
		.pcCmd = "ATE0\r\n",
		.u16CmdSize = sizeof("ATE0\r\n")-1,
		.pcCmdResponseOnOK = "OK",
		.i16TimeoutMs = 500,
		.i16DelayMs = 500,
		.bSkip = false
};

static PPPOS_CmdType UC20_xCmdVerboseResult =
{
		.pcCmd = "AT+CMEE=2\r\n",
		.u16CmdSize = sizeof("AT+CMEE=2\r\n")-1,
		.pcCmdResponseOnOK = "OK",
		.i16TimeoutMs = 500,
		.i16DelayMs = 500,
		.bSkip = false
};

#if UMTS_USE_MUX == 0
static char UC20_CmdUrcPort[] = "AT+QURCCFG=\"URCPORT\",\"uart1\"\r\n";
static PPPOS_CmdType UC20_xCmdUrcPort =
{
		.pcCmd = UC20_CmdUrcPort,
		.u16CmdSize = sizeof(UC20_CmdUrcPort)-1,
		.pcCmdResponseOnOK = "OK",
		.i16TimeoutMs = 500,
		.i16DelayMs = 500,
		.bSkip = false
};
#endif

static PPPOS_CmdType UC20_xCmdCpin =
{
		.pcCmd = "AT+CPIN?\r\n",
		.u16CmdSize = sizeof("AT+CPIN?\r\n")-1,
		.pcCmdResponseOnOK = "CPIN: READY",
		.i16TimeoutMs = 500,
		.i16DelayMs = 500,
		.bSkip = false
};

static PPPOS_CmdType UC20_xCmdCIMI =
{
	.pcCmd = "AT+CIMI\r\n",
	.u16CmdSize = sizeof("AT+CIMI\r\n")-1,
	.pcCmdResponseOnOK = "OK",
	.i16TimeoutMs = 500,
	.i16DelayMs = 500,
	.bSkip = false,
};

static PPPOS_CmdType UC20_xCmdCCID =
{
	.pcCmd = "AT+QCCID\r\n",
	.u16CmdSize = sizeof("AT+QCCID\r\n")-1,
	.pcCmdResponseOnOK = "OK",
	.i16TimeoutMs = 500,
	.i16DelayMs = 500,
	.bSkip = false,
};

static PPPOS_CmdType UC20_xCmdCREG =
{
	.pcCmd = "AT+CREG?\r\n",
	.u16CmdSize = sizeof("AT+CREG?\r\n")-1,
	.pcCmdResponseOnOK = "OK",
	.i16TimeoutMs = 5000,
	.i16DelayMs = 500,
	.bSkip = false,
};

/* To get operator name */
static PPPOS_CmdType UC20_xCmdQueryOperator =
{
	.pcCmd = "AT+COPS?\r\n",
	.u16CmdSize = sizeof("AT+COPS?\r\n")-1,
	.pcCmdResponseOnOK = "OK",
	.i16TimeoutMs = 500,
	.i16DelayMs = 500,
	.bSkip = false,
};

#define DEFAULT_APN_CMD 	"AT+CGDCONT=1,\"IP\",\"\"\r\n"
static PPPOS_CmdType UC20_xCmdSetAPN =
{
		.pcCmd = DEFAULT_APN_CMD,
		.u16CmdSize = sizeof(DEFAULT_APN_CMD)-1,
		.pcCmdResponseOnOK = "OK",
		.i16TimeoutMs = 500,
		.i16DelayMs = 500,
		.bSkip = false,
};

static PPPOS_CmdType UC20_xCmdDial =
{
		.pcCmd = "ATD*99#\r\n",
		.u16CmdSize = sizeof("ATD*99#\r\n")-1,
		.pcCmdResponseOnOK = "CONNECT",
		.i16TimeoutMs = 5000,
		.i16DelayMs = 500,
		.bSkip = false,
};

PPPOS_CmdType *UC20_arrxInitCmds[] =
{
		&UC20_xCmdAT,
		&UC20_xCmdATV1,
		&UC20_xCmdEchoOff,
		&UC20_xCmdRFOn,
		&UC20_xCmdVerboseResult,
#if UMTS_USE_MUX == 0
		&UC20_xCmdUrcPort,
#endif
		&UC20_xCmdCpin
};

PPPOS_CmdType *UC20_arrxConnectCmds[] =
{
		&UC20_xCmdSetAPN,
		&UC20_xCmdDial
};

#define SUPPORTED_OPERATORS_COUNT	2

static char* UC20_SupportedOperators[SUPPORTED_OPERATORS_COUNT] =
{
		"VIETTEL",
		"VINAPHONE",
};

#define INIT_CMD_SIZE  				(sizeof(UC20_arrxInitCmds)/sizeof(PPPOS_CmdType *))
#define CONNECT_CMD_SIZE  			(sizeof(UC20_arrxConnectCmds)/sizeof(PPPOS_CmdType *))

void PPPOS_DoGetSimCCID_UC20(char *pBuffer)
{
	char arrcBuf[PPPOS_BUF_LEN_64] = "";
	PPPOS_ClearRxBuf();

	int iFailCnt = 0, iRes = 0;
	while (iFailCnt <= PPPOS_CMD_MAX_FAIL_COUNT) {
		iRes = Umts_SendAT(UC20_xCmdCCID.pcCmd,
											 UC20_xCmdCCID.pcCmdResponseOnOK,
											 NULL,
											 -1,
											 2000,
											 arrcBuf,
											 63);
		if (iRes > 0) {
			sscanf(arrcBuf, "..+QCCID: %s", pBuffer);
			PPPOS_RemoveSubStr(pBuffer, "....OK..");
			PPPOS_DBG("QCCID = %s\n", pBuffer);
			break;
		}

		PPPOS_SLEEP(PPPOS_SLEEP_ON_CMD_FAIL);
		iFailCnt++;
	}
}

void PPPOS_DoGetSimCIMI_UC20(char *pBuffer)
{
	char arrcBuf[PPPOS_BUF_LEN_64] = "";
	PPPOS_ClearRxBuf();

	int iFailCnt = 0, iRes = 0;

	while (iFailCnt <= PPPOS_CMD_MAX_FAIL_COUNT) {
		iRes = Umts_SendAT(UC20_xCmdCIMI.pcCmd,
											 UC20_xCmdCIMI.pcCmdResponseOnOK,
											 NULL,
											 -1,
											 2000,
											 arrcBuf,
											 63);
		if (iRes > 0) {
			sscanf(arrcBuf, "..%s", pBuffer);
			PPPOS_RemoveSubStr(pBuffer, "....OK..");
			PPPOS_DBG("CIMI = %s\n", pBuffer);
			break;
		}

		PPPOS_SLEEP(PPPOS_SLEEP_ON_CMD_FAIL);
		iFailCnt++;
	}

}

static void PPPOS_StrUpr(char *pcStr)
{
	char *pcIter = pcStr;

	while(*pcIter != NULL) {
		if(*pcIter >= 'a' && *pcIter <= 'z') {
			*pcIter -= 32;
		}
		pcIter++;
	}
}

void PPPOS_DoGetOperator_UC20(void)
{
	char arrcBuf[PPPOS_BUF_LEN_64] = "";
	PPPOS_ClearRxBuf();

	int iFailCnt = 0, iRes = 0;
	while (iFailCnt <= PPPOS_CMD_MAX_FAIL_COUNT) {
		iRes = Umts_SendAT(UC20_xCmdQueryOperator.pcCmd,
											 UC20_xCmdQueryOperator.pcCmdResponseOnOK,
											 NULL,
											 -1,
											 2000,
											 arrcBuf,
											 63);
		if (iRes > 0) {
			PPPOS_StrUpr(arrcBuf);
			for(int i = 0; i < SUPPORTED_OPERATORS_COUNT; i++) {
				if(strstr(arrcBuf, UC20_SupportedOperators[i]) != NULL) {
					PPPOS_DBG("OPERATOR: %s\r\n", UC20_SupportedOperators[i]);
					break;
				}
			}
			break;
		}

		PPPOS_SLEEP(500);
		iFailCnt++;
	}
}

bool PPPOS_DoInitAT_UC20(void)
{
	int iCmdIter = 0, iFailCnt = 0;

	/* GSM Initialization loop */
	while(iCmdIter < INIT_CMD_SIZE) {
		if (UC20_arrxInitCmds[iCmdIter]->bSkip) {
			/* Skip command which has received valid response */
			iCmdIter++;
			continue;
		}

		if (Umts_SendAT(UC20_arrxInitCmds[iCmdIter]->pcCmd,
				UC20_arrxInitCmds[iCmdIter]->pcCmdResponseOnOK,
							   NULL,
							   UC20_arrxInitCmds[iCmdIter]->u16CmdSize,
							   UC20_arrxInitCmds[iCmdIter]->i16TimeoutMs,
		             NULL, 0) == 0)
		{
			/* No response or not as expected, start from first initialization command */
			PPPOS_DBG("[PPPOS] Retrying...\n");

			iFailCnt++;
			if (iFailCnt > PPPOS_CMD_MAX_FAIL_COUNT) {
				return false;
			}

			PPPOS_SLEEP(PPPOS_SLEEP_ON_CMD_FAIL);
			iCmdIter = 0;
			continue;
		}

		iFailCnt = 0;
		/* Delay before next command */
		if (UC20_arrxInitCmds[iCmdIter]->i16DelayMs > 0) {
			PPPOS_SLEEP(UC20_arrxInitCmds[iCmdIter]->i16DelayMs);
		}

		/* Skip this command for next loops as it has been executed done*/
		UC20_arrxInitCmds[iCmdIter]->bSkip = true;

		if (UC20_arrxInitCmds[iCmdIter] == &UC20_xCmdCREG) {
			UC20_arrxInitCmds[iCmdIter]->i16DelayMs = 0;
		}

		/* Next command */
		iCmdIter++;
	}

	PPPOS_DoGetOperator_UC20();

	PPPOS_DBG("[PPPOS] GSM initialized.\n");

	return true;

}

bool PPPOS_DoConnectAT_UC20(void)
{
	int iCmdIter = 0, iFailCnt = 0;

	/* connect */
	PPPOS_DBG("[PPPOS] GSM Connecting...\n");

	/* GSM Connecting loop */
	while(iCmdIter < CONNECT_CMD_SIZE) {
		if (UC20_arrxConnectCmds[iCmdIter]->bSkip) {
			/* Skip */
			iCmdIter++;
			continue;
		}

		if (Umts_SendAT(UC20_arrxConnectCmds[iCmdIter]->pcCmd,
				UC20_arrxConnectCmds[iCmdIter]->pcCmdResponseOnOK, NULL,
				UC20_arrxConnectCmds[iCmdIter]->u16CmdSize,
				UC20_arrxConnectCmds[iCmdIter]->i16TimeoutMs,
		    NULL, 0) == 0)
		{
			/* No response or not as expected, start from first initialization command */
			PPPOS_DBG("[PPPOS] Retrying...\n");

			iFailCnt++;
			if (iFailCnt > PPPOS_CMD_MAX_FAIL_COUNT) {
				return false;
			}

			PPPOS_SLEEP(PPPOS_SLEEP_ON_CMD_FAIL);
			iCmdIter = 0;
			continue;
		}

		iFailCnt = 0;
		if (UC20_arrxConnectCmds[iCmdIter]->i16DelayMs > 0) {
			PPPOS_SLEEP(UC20_arrxConnectCmds[iCmdIter]->i16DelayMs);
		}

		UC20_arrxConnectCmds[iCmdIter]->bSkip = true;
		if (UC20_arrxConnectCmds[iCmdIter] == &UC20_xCmdCREG) {
			UC20_arrxConnectCmds[iCmdIter]->i16DelayMs = 0;
		}
		/* Next command */
		iCmdIter++;
	}

	return true;
}

void PPPOS_DoDisconnectAT_UC20(bool bRfOff)
{
	/* Check if modem is in command mode */
	PPPOS_DBG("Disconnect GSM modem\r\n");

	int iRes = Umts_SendAT(PPPOS_CMD_AT, PPPOS_OK_STR, NULL,
									 sizeof(PPPOS_CMD_AT)-1, PPPOS_CMD_AT_TIMEOUT, NULL, 0);
	if (iRes == 1) {
		if (bRfOff) {
			PPPOS_SetRFOff_UC20();
		}
		return;
	}

	/* Currently in data mode (online) */
	PPPOS_DBG("[PPPOS] ONLINE, DISCONNECTING...\n");
	PPPOS_SLEEP(DISCONNECT_SLEEP_ON_RETRY);
	Serial_SendUART((unsigned char*)SWITCH_MODE_STR, strlen(SWITCH_MODE_STR));
	PPPOS_SLEEP(DISCONNECT_SLEEP_ON_RETRY);

	/* Trying to switch to command mode */
	int n = 0;
	iRes = Umts_SendAT(PPPOS_CMD_ATH, PPPOS_OK_STR, PPPOS_CMD_ATH_NO_CARRIER_RESP,
								sizeof(PPPOS_CMD_ATH)-1, PPPOS_CMD_ATH_TIMEOUT, NULL, 0);
	while (iRes == 0) {
		if (++n > DISCONNECT_SEND_TEST_COUNT) {
			PPPOS_DBG("[PPPOS] STILL CONNECTED.\n");
			n = 0;
			PPPOS_SLEEP(DISCONNECT_SLEEP_ON_RETRY);
			Serial_SendUART((unsigned char*)SWITCH_MODE_STR, strlen(SWITCH_MODE_STR));
			PPPOS_SLEEP(DISCONNECT_SLEEP_ON_RETRY);
		}

		PPPOS_SLEEP(500);
		iRes = Umts_SendAT(PPPOS_CMD_ATH, PPPOS_OK_STR, PPPOS_CMD_ATH_NO_CARRIER_RESP,
									 sizeof(PPPOS_CMD_ATH)-1, PPPOS_CMD_ATH_TIMEOUT, NULL, 0);
	}

	/* Disconnected, turn off RF if required */
	PPPOS_SLEEP(DISCONNECT_SLEEP_ON_RETRY);
	if (bRfOff) {
		PPPOS_SetRFOff_UC20();
	}

	PPPOS_SLEEP(1000);
	PPPOS_ClearRxBuf();

	PPPOS_DBG("[PPPOS] DISCONNECTED.\n");
}

void PPPOS_EnableInitCmds_UC20(void)
{
	for (int idx = 0; idx < INIT_CMD_SIZE; idx++) {
		UC20_arrxInitCmds[idx]->bSkip = false;
	}
}

void PPPOS_EnableConnectCmds_UC20(void)
{
	for (int idx = 0; idx < CONNECT_CMD_SIZE; idx++) {
		UC20_arrxConnectCmds[idx]->bSkip = false;
	}
}

int PPPOS_SetRFOff_UC20(void)
{
	UC20_xCmdCREG.i16TimeoutMs = PPPOS_CMD_RFOFF_TIMEOUT;
	int iRes = Umts_SendAT(PPPOS_CMD_RFOFF, PPPOS_OK_STR, NULL, sizeof(PPPOS_CMD_RFOFF)-1,
								    PPPOS_CMD_RFOFF_TIMEOUT, NULL, 0);
    return iRes;
}

int PPPOS_SetRFOn_UC20(void)
{
	UC20_xCmdCREG.i16TimeoutMs = PPPOS_CMD_RFON_TIMEOUT;
	int iRes = Umts_SendAT(PPPOS_CMD_RFON, PPPOS_OK_STR, NULL, sizeof(PPPOS_CMD_RFON)-1,
								    PPPOS_CMD_RFON_TIMEOUT, NULL, 0);
    return iRes;
}


#endif
