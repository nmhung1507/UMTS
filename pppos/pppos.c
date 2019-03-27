#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

#include "ppp/pppos.h"
#include "ppp/ppp.h"
#include "pppapi.h"
#include "lwip.h"

#include "pppos.h"
#include "pppos_cfg.h"
#include "umts_serial.h"
#include "usart.h"

#if (PPPOS_CFG_MODEM_UBLOX > 0) && (PPPOS_CFG_MODEM_UC20 > 0)
#error "SELECT ONLY 1 MODEL"
#endif

static const int iRxBufSize = 128;

/* Private function prototypes */
static void 	PPPOS_PPPStatusCallback(ppp_pcb *pxPcb, int iErrCode, void *pvCtx);
static u32_t 	PPPOS_PPPOutputCallback(ppp_pcb *pxPcb, u8_t *pu8Data, u32_t u32Len, void *pvCtx);
static void     PPPOS_EnableInitCmds(void);
static void     PPPOS_EnableConnectCmds(void);
static void     PPPOS_MainTask(const void * pvArgument);
static void 	PPPOS_WaitDataEvent(void);
static void 	PPPOS_CreateEventGroup(void);


/* Shared variables, use mutex to access them */
PPPOS_HandleType	PPPOS_xHandle = {
	.State	= GSM_STATE_FIRSTINIT,			/* current GSM state */
	.iDoConnect = 1,
	.u32RxCount = 0,						/* rx data counter */
	.u32TxCount = 0,						/* tx data counter */
	.bTaskStarted = false,
	.bRfOff = 0
};

/* Local variables */
static QueueHandle_t ppos_Mutex = NULL;
const char *PPP_User = "";
const char *PPP_Pass = "";

static char PPPOS_arrcCCID[PPPOS_CCID_BUF_LEN] = "";
static char PPPOS_arrcCIMI[PPPOS_CIMI_BUF_LEN] = "";

/* The PPP control block */
static ppp_pcb *PPPOS_Pcb = NULL;

/* The PPP IP interface */
struct netif PPPOS_gPPPNetIf;

extern xQueueHandle					gMainUartQueue;

EventGroupHandle_t 			PPPOS_RxDataEventGroup;		/* Rx data event group */

static PPPOS_ConnectedCallbackType	PPPOS_pfnConnectedCallback 	= NULL;
static PPPOS_ErrorCallbackType		PPPOS_pfnErrorCallback 		= NULL;

/**
 * @brief Set connected callback 
 * 
 * @param pfn 
 */
void PPPOS_SetConnectedCallback(PPPOS_ConnectedCallbackType pfn)
{
	if(pfn) {
		PPPOS_pfnConnectedCallback = pfn;
	}
}

/**
 * @brief Set error callback
 * 
 * @param pfn 
 */
void PPPOS_SetErrorCallback(PPPOS_ErrorCallbackType pfn)
{
	if(pfn) {
		PPPOS_pfnErrorCallback = pfn;
	}
}

/**
 * @brief	PPP status callback 
 */
static void PPPOS_PPPStatusCallback(ppp_pcb *pxPcb, int iErrCode, void *pvCtx)
{
	struct netif *pxPPPIf = ppp_netif(pxPcb);
	LWIP_UNUSED_ARG(pvCtx);

	switch(iErrCode) {
		case PPPERR_NONE: {
			PPPOS_DBG("[PPPOS] status_cb: Connected\n");
			#if PPP_IPV4_SUPPORT
			PPPOS_DBG("[PPPOS]   ipaddr    = %s\n", ipaddr_ntoa(&pxPPPIf->ip_addr));
			PPPOS_DBG("[PPPOS]   gateway   = %s\n", ipaddr_ntoa(&pxPPPIf->gw));
			PPPOS_DBG("[PPPOS]   netmask   = %s\n", ipaddr_ntoa(&pxPPPIf->netmask));

			#if PPP_IPV6_SUPPORT
			PPPOS_DBG("   ip6addr   = %s\n", ip6addr_ntoa(netif_ip6_addr(pxPPPIf, 0)));
			#endif	/*PPP_IPV6_SUPPORT*/
			#endif	/*PPP_IPV4_SUPPORT*/

			PPPOS_Lock();
			PPPOS_xHandle.State = GSM_STATE_CONNECTED;
			PPPOS_Unlock();
			
			if(PPPOS_pfnConnectedCallback) {
				PPPOS_pfnConnectedCallback();
			}
			break;
		}
		case PPPERR_PARAM: {
			PPPOS_DBG("[PPPOS] status_cb: Invalid parameter\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_OPEN: {
			PPPOS_DBG("[PPPOS] status_cb: Unable to open PPP session\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_DEVICE: {
			PPPOS_DBG("[PPPOS] status_cb: Invalid I/O device for PPP\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_ALLOC: {
			PPPOS_DBG("[PPPOS] status_cb: Unable to allocate resources\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_USER: {
			/* ppp_free(); -- can be called here */
			PPPOS_DBG("[PPPOS] status_cb: User interrupt (disconnected)\n");
			PPPOS_Lock();
			PPPOS_xHandle.State = GSM_STATE_DISCONNECTED;
			PPPOS_Unlock();
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_CONNECT: {
			PPPOS_DBG("[PPPOS] status_cb: Connection lost\n");
			PPPOS_Lock();
			PPPOS_xHandle.State = GSM_STATE_DISCONNECTED;
			PPPOS_Unlock();
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_AUTHFAIL: {
			PPPOS_DBG("[PPPOS] status_cb: Failed authentication challenge\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_PROTOCOL: {
			PPPOS_DBG("[PPPOS] status_cb: Failed to meet protocol\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_PEERDEAD: {
			PPPOS_DBG("[PPPOS] status_cb: Connection timeout\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_IDLETIMEOUT: {
			PPPOS_DBG("[PPPOS] status_cb: Idle Timeout\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_CONNECTTIME: {
			PPPOS_DBG("[PPPOS] status_cb: Max connect time reached\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		case PPPERR_LOOPBACK: {
			PPPOS_DBG("[PPPOS] status_cb: Loopback detected\n");
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
		default: {
			PPPOS_DBG("[PPPOS] status_cb: Unknown error code %d\n", iErrCode);
			if(PPPOS_pfnErrorCallback) {
				PPPOS_pfnErrorCallback(iErrCode);
			}
			break;
		}
	}
}

/**
 * @brief 	Handle sending data to GSM modem 
 */
static u32_t PPPOS_PPPOutputCallback(ppp_pcb *pxPcb, u8_t *pu8Data, u32_t u32Len, void *pvCtx)
{
	bool ret = Serial_SendUART((unsigned char*)pu8Data, u32Len);
	if (ret == true) {
#if PPPOS_CFG_COUNTERS_EN
		PPPOS_Lock();
		PPPOS_xHandle.u32RxCount += u32Len;
		PPPOS_Unlock();			
#endif
		return u32Len;
	}

	return 0;	
}

void PPPOS_Lock(void)
{
	PPPOS_LOCK(ppos_Mutex, PPPOS_CFG_MTX_TIMEOUT);
}

void PPPOS_Unlock(void)
{
	PPPOS_UNLOCK(ppos_Mutex);
}

/**
 * @brief Get GSM status
 *
 * @return int
 */
int PPPOS_GetStatus(void)
{
	PPPOS_Lock();
	PPPOS_StateType CurrState = PPPOS_xHandle.State;
	PPPOS_Unlock();

	return CurrState;
}

/**
 * @brief Get Tx & Rx counters
 *
 * @param rx
 * @param tx
 * @param rst - reset counters flag
 */
void PPPOS_GetDataCounters(uint32_t *pu32Rx, uint32_t *pu32Tx, bool bRst)
{
	PPPOS_Lock();
	*pu32Rx = PPPOS_xHandle.u32RxCount;
	*pu32Tx = PPPOS_xHandle.u32TxCount;

	if (bRst) {
		PPPOS_xHandle.u32RxCount = 0;
		PPPOS_xHandle.u32TxCount = 0;
	}
	PPPOS_Unlock();
}

/**
 * @brief Reset tx & rx counters
 *
 */
void PPPOS_ResetDataCounters(void)
{
#if PPPOS_CFG_COUNTERS_EN
	PPPOS_Lock();
	PPPOS_xHandle.u32RxCount = 0;
	PPPOS_xHandle.u32TxCount = 0;
	PPPOS_Unlock();
#endif
}

/**
 * @brief Turn off GSM RF
 *
 * @return int
 */
int PPPOS_SetRFOff(void)
{
#if PPPOS_CFG_MODEM_UBLOX > 0
	return PPPOS_SetRFOff_Ublox();
#elif PPPOS_CFG_MODEM_UC20 > 0
	return PPPOS_SetRFOff_UC20();
#endif
}

/**
 * @brief Turn off GSM RF
 *
 * @return int
 */
int PPPOS_SetRFOn(void)
{
#if PPPOS_CFG_MODEM_UBLOX > 0
	return PPPOS_SetRFOn_Ublox();
#elif PPPOS_CFG_MODEM_UC20 > 0
	return PPPOS_SetRFOn_UC20();
#endif
}


/**
 * @brief	Show command info 
 */
void PPPOS_ShowCmdInfo(const char *pcCmd, int iCmdSize, const char *pcInfo)
{
	char arrcBuf[iCmdSize + 2];
	memset(arrcBuf, 0, iCmdSize + 2);

	for (int idx = 0; idx < iCmdSize; idx++) {
		/* Remove non-valid characters */
		if ((pcCmd[idx] != NULL_CHAR) && ((pcCmd[idx] < SPACE_CHAR) || (pcCmd[idx] >= DEL_CHAR))) {
			arrcBuf[idx] = DOT_CHAR;
		}
		else { 
			arrcBuf[idx] = pcCmd[idx];
		}

		if (arrcBuf[idx] == NULL_CHAR) {
			break;
		}
	}

	PPPOS_DBG("[PPPOS] %s [%s]\n", pcInfo, arrcBuf);
}

/**
 * @brief	Clear rx buffer
 */
int PPPOS_ClearRxBuf(void)
{
	char arrcData[PPPOS_RESPONSE_BUF_LEN] = {NULL_CHAR};
  	int iLen;

	PPPOS_Lock();
	int iCurrState = PPPOS_xHandle.State;
	PPPOS_Unlock();

	if ((iCurrState != GSM_STATE_FIRSTINIT) && (iCurrState != GSM_STATE_IDLE)) {
		return 0;
	}

	while (1) {
		iLen = Serial_RecvUART(arrcData, PPPOS_RESPONSE_BUF_LEN);
		if (iLen <= 0) {
			break;
		}
		PPPOS_SLEEP(1);
	}

	return 1;
}

/**
 * @brief Disconnect from GSM 
 * 
 * @param rfOff 
 */
void PPPOS_DoDisconnectAT(bool bRfOff)
{
#if PPPOS_CFG_MODEM_UBLOX > 0
	PPPOS_DoDisconnectAT_Ublox(bRfOff);
#elif PPPOS_CFG_MODEM_UC20 > 0
	PPPOS_DoDisconnectAT_UC20(bRfOff);
#endif
}


/**
 * @brief Enable all init commands 
 * 
 */
static void PPPOS_EnableInitCmds(void)
{
#if PPPOS_CFG_MODEM_UBLOX > 0
	PPPOS_EnableInitCmds_Ublox();
#elif PPPOS_CFG_MODEM_UC20 > 0
	PPPOS_EnableInitCmds_UC20();
#endif
}

/**
 * @brief Enable all connect commands
 * 
 */
static void PPPOS_EnableConnectCmds(void)
{
#if PPPOS_CFG_MODEM_UBLOX > 0
	PPPOS_EnableConnectCmds_Ublox();
#elif PPPOS_CFG_MODEM_UC20 > 0
	PPPOS_EnableConnectCmds_UC20();
#endif
}

/**
 * @brief Get SIM information 
 * 
 * @param ccid 
 * @param cimi 
 */
void PPPOS_GetSIMInfo(char *pcCCID, char *pcCIMI)
{
	if ((pcCCID == NULL) || (pcCIMI == NULL))
		return;
	
	memcpy(pcCCID, PPPOS_arrcCCID, PPPOS_CCID_BUF_LEN);
	memcpy(pcCIMI, PPPOS_arrcCIMI, PPPOS_CIMI_BUF_LEN);
}

void PPPOS_RemoveSubStr (char *pcString, char *pcSub)
{
    char *pcMatch = pcString;
    int iLen = strlen(pcSub);
    while ((pcMatch = strstr(pcMatch, pcSub))) {
        *pcMatch = NULL_CHAR;
        strcat(pcString, pcMatch+iLen);
		pcMatch++;
    }
}

/**
 * @brief	Send AT commands to init modem
 */
static bool PPPOS_DoInitAT(void)
{
#if PPPOS_CFG_MODEM_UBLOX > 0
	return PPPOS_DoInitAT_Ublox();
#elif PPPOS_CFG_MODEM_UC20 > 0
	return PPPOS_DoInitAT_UC20();
#else
	return false;
#endif
}

/**
 * @brief	Send AT command to connect modem
 */
static bool PPPOS_DoConnectAT(void)
{
#if PPPOS_CFG_MODEM_UBLOX > 0
	return PPPOS_DoConnectAT_Ublox();
#elif PPPOS_CFG_MODEM_UC20 > 0
	return PPPOS_DoConnectAT_UC20();
#else
	return false;
#endif

}

void PPPOS_DoGetSimCCID(void)
{
#if PPPOS_CFG_MODEM_UBLOX > 0
	PPPOS_DoGetSimCCID_Ublox(PPPOS_arrcCCID);
#elif PPPOS_CFG_MODEM_UC20 > 0
	PPPOS_DoGetSimCCID_UC20(PPPOS_arrcCCID);
#endif
}

void PPPOS_DoGetSimCIMI(void)
{
#if PPPOS_CFG_MODEM_UBLOX > 0
	PPPOS_DoGetSimCIMI_Ublox(PPPOS_arrcCIMI);
#elif PPPOS_CFG_MODEM_UC20 > 0
	PPPOS_DoGetSimCIMI_UC20(PPPOS_arrcCIMI);
#endif
}

/**
 * @brief PPPoS TASK - Handles GSM initialization,
 * disconnection and GSM modem responses
 * 
 * @param argument 
 */
static void PPPOS_MainTask(const void * pvArgument)
{
	PPPOS_Lock();
	PPPOS_xHandle.bTaskStarted = true;
	PPPOS_Unlock();

  	/* Allocate receive buffer */
  	char* pcData = (char*) pvPortMalloc(iRxBufSize);
	if (pcData == NULL) {
		PPPOS_DBG("[PPPOS] Failed to allocate data buffer.\n");
		goto exit;
  	}
		
	/* Create event group */
	PPPOS_CreateEventGroup();

	PPPOS_Lock();
#if PPPOS_CFG_COUNTERS_EN
    PPPOS_xHandle.u32TxCount = 0;
    PPPOS_xHandle.u32RxCount = 0;
#endif
	PPPOS_xHandle.State = GSM_STATE_FIRSTINIT;
	PPPOS_Unlock();

	while(1) {
		/* Disconnect if connected */
		PPPOS_DoDisconnectAT(true);

		/* Init all commands */
		PPPOS_EnableInitCmds();
		PPPOS_EnableConnectCmds();

		PPPOS_DBG("[PPPOS] GSM initialization start\n");

		/* Init Modem */
		if(PPPOS_DoInitAT() != true) {
			goto exit;
		}
		
		/* Get CCID & CIMI */
		PPPOS_DoGetSimCCID();
		PPPOS_DoGetSimCIMI();

		/* Connect */
		if(PPPOS_DoConnectAT() != true) {
			goto exit;
		}

		PPPOS_Lock();
		if (PPPOS_xHandle.State == GSM_STATE_FIRSTINIT) {
			/* LWIP must have been initialized already before init GSM */			
			PPPOS_Unlock();

			/* After first successful initialization create PPP control block */
			PPPOS_Pcb = pppapi_pppos_create(&PPPOS_gPPPNetIf, PPPOS_PPPOutputCallback, 
                                            PPPOS_PPPStatusCallback, NULL);

			if (PPPOS_Pcb == NULL) {
				PPPOS_DBG("[PPPOS] Error initializing PPPoS\n");
				break; /* end task */
			}
		}
		else {
			PPPOS_Unlock();
		}

		pppapi_set_default(PPPOS_Pcb);
		//pppapi_set_auth(ppp, PPPAUTHTYPE_PAP, PPP_User, PPP_Pass);
		//pppapi_set_auth(ppp, PPPAUTHTYPE_NONE, PPP_User, PPP_Pass);
		//ppp_set_usepeerdns(ppp, 1);

		PPPOS_Lock();
		/* State will be updated on status callback after call pppapi_connect */
		PPPOS_xHandle.State = GSM_STATE_IDLE;
		PPPOS_Unlock();

		pppapi_connect(PPPOS_Pcb, 0);

		/* LOOP: Handle GSM modem responses & disconnects */
		while(1) {
			/* Check if disconnect requested */
			PPPOS_Lock();
			if (PPPOS_xHandle.iDoConnect <= 0) {
				int iEndTask = PPPOS_xHandle.iDoConnect;
				PPPOS_xHandle.iDoConnect = 1;
				PPPOS_Unlock();

				PPPOS_DBG("\r\n[PPPOS] Disconnect requested.\n");

				pppapi_close(PPPOS_Pcb, 0);

				while (PPPOS_xHandle.State != GSM_STATE_DISCONNECTED) {
					/* Handle data received from GSM */
					memset(pcData, 0, iRxBufSize);
					int iLen = Serial_RecvUART(pcData, iRxBufSize);
					if (iLen > 0) {
						pppos_input_tcpip(PPPOS_Pcb, (u8_t*)pcData, iLen);
						#if PPPOS_CFG_COUNTERS_EN
						PPPOS_Lock();
					    PPPOS_xHandle.u32TxCount += iLen;
						PPPOS_Unlock();
						#endif
					}
				}

				PPPOS_Lock();
				bool bRfOff = PPPOS_xHandle.bRfOff;
				PPPOS_Unlock();

				/* Disconnect GSM if still connected */
				PPPOS_DoDisconnectAT(bRfOff);

				PPPOS_DBG("[PPPOS] Disconnected.\n");

				PPPOS_EnableInitCmds();
				PPPOS_EnableConnectCmds();

				PPPOS_Lock();
				PPPOS_xHandle.State = GSM_STATE_IDLE;
				PPPOS_xHandle.iDoConnect = 0;
				PPPOS_Unlock();

				if (iEndTask < 0) {
					goto exit;
				}

				/* Wait for reconnect request */
				int iCurrState = GSM_STATE_DISCONNECTED;
				while (iCurrState == GSM_STATE_DISCONNECTED) {
					PPPOS_SLEEP(PPPOS_SLEEP_10MS);
					PPPOS_Lock();
					iCurrState = PPPOS_xHandle.iDoConnect;
					PPPOS_Unlock();
				}

				PPPOS_DBG("\r\n[PPPOS] Reconnect requested.\n");
				break;
			}

			/* Check if disconnected */
			if (PPPOS_xHandle.State == GSM_STATE_DISCONNECTED) {
				PPPOS_Unlock();
				PPPOS_DBG("\r\n[PPPOS] Disconnected, trying again...\n");
				pppapi_close(PPPOS_Pcb, 0);

				PPPOS_EnableInitCmds();
				PPPOS_EnableConnectCmds();
				PPPOS_xHandle.State = GSM_STATE_IDLE;
				PPPOS_SLEEP(PPPOS_SLEEP_ON_CMD_FAIL*5);
				break;
			}
			else {
				PPPOS_Unlock();
			}

			/* Handle data received from GSM & send to LWIP */
			memset(pcData, 0, iRxBufSize);
			int iLen = Serial_RecvUART(pcData, iRxBufSize);
			if (iLen > 0) {
				pppos_input_tcpip(PPPOS_Pcb, (u8_t*)pcData, iLen);
#if PPPOS_CFG_COUNTERS_EN
				PPPOS_Lock();
			    PPPOS_xHandle.u32TxCount += iLen;
				PPPOS_Unlock();
#endif
			}
			else {
				/* Wait for UART rx event -> continue this loop & receive */
//				PPPOS_WaitDataEvent();
				osDelay(3);
			}
		}  /* Handle GSM modem responses & disconnects loop */
	}

exit:
	if (pcData) {
		vPortFree(pcData);  /* free data buffer */
	}

	if (PPPOS_Pcb) {
		ppp_free(PPPOS_Pcb);
	}

	PPPOS_Lock();
	PPPOS_xHandle.bTaskStarted = false;
	PPPOS_xHandle.State = GSM_STATE_FIRSTINIT;
	PPPOS_Unlock();

	PPPOS_DBG("[PPPOS] PPPoS TASK TERMINATED\n");

	vTaskDelete(NULL);
}

/**
 * @brief Init GSM 
 * @param bWaitUntilConnected
 * @return int 
 */
int PPPOS_Init(bool bWaitUntilConnected)
{
	/* If called not by the first time */
	if (ppos_Mutex != NULL) {
		PPPOS_Lock();
	}

	/* Configure MUX if enabled */
#if UMTS_USE_MUX > 0
//	Mux_Init();
	Mux_DLCSetRecvDataCallback(PPPOS_CFG_UC20_DLCI, PPPOS_PushDataToQueue);
#endif

	PPPOS_xHandle.iDoConnect = 1;
	PPPOS_StateType CurrState = GSM_STATE_DISCONNECTED;
	bool taskStarted = PPPOS_xHandle.bTaskStarted;

	if (ppos_Mutex != NULL) {
		PPPOS_Unlock();
	}

	/* If task has not been started -> create task...*/
	if (taskStarted == false) {
		/* Create mutex on first time called */
		if (ppos_Mutex == NULL) {
			ppos_Mutex = xSemaphoreCreateMutex();
		}

		if (ppos_Mutex == NULL) {
			return 0;
		}

		osThreadDef(PPPOS_MainTask, PPPOS_MainTask, osPriorityNormal, 
                    0, configMINIMAL_STACK_SIZE * 10);
		osThreadCreate(osThread(PPPOS_MainTask), NULL);
		
		/* Wait for task started */
		while (taskStarted == false) {
			PPPOS_SLEEP(PPPOS_SLEEP_10MS);
			PPPOS_Lock();
			taskStarted = PPPOS_xHandle.bTaskStarted;
			PPPOS_Unlock();
		}
	}
	/* If task has been started, set state to disconnect to reinit */
	else {
		PPPOS_Lock();
		PPPOS_xHandle.State = GSM_STATE_DISCONNECTED;
		PPPOS_Unlock();
	}

	if(bWaitUntilConnected) {
		/* Wait until connected */
		while (CurrState != GSM_STATE_CONNECTED) {
			PPPOS_SLEEP(PPPOS_SLEEP_10MS);
			PPPOS_Lock();
			CurrState = PPPOS_xHandle.State;
			taskStarted = PPPOS_xHandle.bTaskStarted;
			PPPOS_Unlock();
			if (taskStarted == false) {
				return 0;
			}
		}

		return 1;
	}
	else {
		return 1;
	}
}

/**
 * @brief Disconnect from GSM 
 * 
 * @param end_task 
 * @param rfoff 
 */
void PPPOS_Disconnect(bool bEndTask, bool bRfOff)
{
	PPPOS_Lock();
	int iCurrState = PPPOS_xHandle.State;
	PPPOS_Unlock();

	if (iCurrState == GSM_STATE_IDLE) {
		return;
	}

	iCurrState = GSM_STATE_DISCONNECTED;

	PPPOS_Lock();
	if (bEndTask) {
		PPPOS_xHandle.iDoConnect = -1;
	}
	else {
		PPPOS_xHandle.iDoConnect = 0;
	}

	PPPOS_xHandle.bRfOff = bRfOff;
	PPPOS_Unlock();

	while (iCurrState == GSM_STATE_DISCONNECTED) {
		PPPOS_Lock();
		iCurrState = PPPOS_xHandle.iDoConnect;
		PPPOS_Unlock();
		PPPOS_SLEEP(PPPOS_SLEEP_10MS);
	}

	while (iCurrState != GSM_STATE_DISCONNECTED) {
		PPPOS_SLEEP(PPPOS_SLEEP_10MS);
		PPPOS_Lock();
		iCurrState = PPPOS_xHandle.iDoConnect;
		PPPOS_Unlock();
	}
}

/******************* Configure UART ***********************/
/**
 * @brief Create event group
 *
 * @return true
 * @return false
 */
static void PPPOS_CreateEventGroup(void)
{
	PPPOS_RxDataEventGroup = xEventGroupCreate();
}


/**
 * @brief Wait data received signal from ISR
 * 
 */
static void PPPOS_WaitDataEvent(void)
{
	xEventGroupWaitBits(PPPOS_RxDataEventGroup, GSM_DATA_RECEIVED_EVENT,
						pdTRUE, pdFALSE, portMAX_DELAY);
//	huart1.Instance->TDR = (uint8_t) '*';
}


static void PPPOS_TriggerDataEvent(void)
{
	xEventGroupSetBits(PPPOS_RxDataEventGroup, GSM_DATA_RECEIVED_EVENT);
}

bool PPPOS_PushDataToQueue(uint8_t *pu8Data, int iSize)
{
	bool bRet = true, bHasData = false;
  printf("PPOSPDTQ ");
	for(int idx = 0; idx < iSize; idx++) {
		if(xQueueSendToBack(gMainUartQueue, &pu8Data[idx], 0) != pdTRUE) {
			bRet = false;
			break;
		}
		else {
			bHasData = true;
		}
	}

	if(bHasData) {
		PPPOS_TriggerDataEvent();
	}

	return bRet;
}
