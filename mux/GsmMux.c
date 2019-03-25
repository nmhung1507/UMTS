/*
 * GSMMux.c
 *
 *  Created on: Mar 19, 2019
 *      Author: v.daipv1@vinfast.vn
 */

#include "GsmMux.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "task.h"
#include "umts_serial.h"

#ifndef min
#define min(a, b)   ((a < b) ? a : b)
#endif

static const uint8_t Mux_arru8CrcTable[] =
{
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, 0x0E, 0x9F, 0xED,
    0x7C, 0x09, 0x98, 0xEA, 0x7B, 0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A,
    0xF8, 0x69, 0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67, 0x38,
    0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D, 0x36, 0xA7, 0xD5, 0x44,
    0x31, 0xA0, 0xD2, 0x43, 0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0,
    0x51, 0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F, 0x70, 0xE1,
    0x93, 0x02, 0x77, 0xE6, 0x94, 0x05, 0x7E, 0xEF, 0x9D, 0x0C, 0x79,
    0xE8, 0x9A, 0x0B, 0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19,
    0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17, 0x48, 0xD9, 0xAB,
    0x3A, 0x4F, 0xDE, 0xAC, 0x3D, 0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0,
    0xA2, 0x33, 0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21, 0x5A,
    0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F, 0xE0, 0x71, 0x03, 0x92,
    0xE7, 0x76, 0x04, 0x95, 0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A,
    0x9B, 0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89, 0xF2, 0x63,
    0x11, 0x80, 0xF5, 0x64, 0x16, 0x87, 0xD8, 0x49, 0x3B, 0xAA, 0xDF,
    0x4E, 0x3C, 0xAD, 0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1, 0xCA, 0x5B, 0x29,
    0xB8, 0xCD, 0x5C, 0x2E, 0xBF, 0x90, 0x01, 0x73, 0xE2, 0x97, 0x06,
    0x74, 0xE5, 0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB, 0x8C,
    0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9, 0x82, 0x13, 0x61, 0xF0,
    0x85, 0x14, 0x66, 0xF7, 0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C,
    0xDD, 0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3, 0xB4, 0x25,
    0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1, 0xBA, 0x2B, 0x59, 0xC8, 0xBD,
    0x2C, 0x5E, 0xCF,
};

static uint8_t 	Mux_CalCRC(const uint8_t *pu8Input, int iLen);
static void 	Mux_DataWait(void);

static void 	Mux_BufferInit(Mux_BufferType *pxBuffer);
static bool 	Mux_BufferGetFrame(Mux_BufferType *pxBuffer, Mux_FrameType *pFrame);
static uint32_t Mux_BufferWrite(Mux_BufferType *pBuffer, const uint8_t *pData, uint32_t iSize);
static uint32_t Mux_BufferGetLength(Mux_BufferType *pBuffer);
static uint32_t Mux_BufferGetFree(Mux_BufferType *pBuffer);

static void 	Mux_SendUA(Mux_FrameType *pxFrame);
static void 	Mux_MainTask(void const *pvArgument);

static bool 	Mux_FrameCreate(const Mux_FrameInfoType *pxInfo, uint8_t *pu8Frame,
                   	   	   	   uint16_t u16MaxSize, uint16_t *pu16FrameSize);
static void     Mux_FrameProcess(Mux_FrameType *pFrame);

static void 	Mux_DLCDataProcess(uint8_t u8Idx, uint8_t *pu8Data, int iSize);
static void 	Mux_DLC0CmdProcess(uint8_t *pu8Data, int iSize);

/************************ MUX BUFFER & FRAME PROCESS **************************/
/**
 * @brief	Calculate CRC
 */
static uint8_t Mux_CalCRC(const uint8_t *pu8Input, int iLen)
{
    uint8_t u8Fcs = 0xFF;
    for(int i = 0; i < iLen; i++) {
        u8Fcs = Mux_arru8CrcTable[u8Fcs ^ pu8Input[i]];
    }

    return 0xFF - u8Fcs;
}

/**
 * @brief	Create Mux frame
 */
static bool Mux_FrameCreate(const Mux_FrameInfoType *pxInfo, uint8_t *pu8Frame,
                   	   	    uint16_t u16MaxSize, uint16_t *pu16FrameSize)
{
    if(pxInfo == NULL || pu8Frame == NULL) {
        GSMMUX_DBG("INVALID INFO\r\n");
        return false;
    }

    pu8Frame[MUX_FLAG_IDX] = MUX_FLAG;
    pu8Frame[MUX_ADDR_IDX] = pxInfo->u8Addr;
    pu8Frame[MUX_CTRL_IDX] = pxInfo->u8Control;

    uint8_t u8LenByteCount;

    if(pxInfo->u16Length <= 127) {
        pu8Frame[MUX_LEN_IDX] = 1 | (pxInfo->u16Length << 1);
        u8LenByteCount = 1;
    }
    else {
        pu8Frame[MUX_LEN_IDX] = ((uint8_t) (pxInfo->u16Length & 0x7F)) << 1;
        pu8Frame[MUX_LEN_IDX + 1] = (uint8_t) (pxInfo->u16Length >> 7);
        u8LenByteCount = 2;
    }

    int iFrameLength = 5 + u8LenByteCount + pxInfo->u16Length;
    if(iFrameLength > u16MaxSize) {
		GSMMUX_DBG("Frame length exceed (%d/%d)!\r\n", iFrameLength, u16MaxSize);
        return false;
    }

    /* Data */
    if(pxInfo->u16Length > 0) {
        memcpy(&pu8Frame[MUX_LEN_IDX+u8LenByteCount], pxInfo->pu8Data, pxInfo->u16Length);
    }

    uint8_t arru8CksumData[4];
    uint8_t u8CksumLen;

    arru8CksumData[0] = pxInfo->u8Addr;
    arru8CksumData[1] = pxInfo->u8Control;
    if(u8LenByteCount == 1) {
        arru8CksumData[2] = pu8Frame[MUX_LEN_IDX];
    }
    else {
        arru8CksumData[2] = pu8Frame[MUX_LEN_IDX];
        arru8CksumData[3] = pu8Frame[MUX_LEN_IDX + 1];
    }

    u8CksumLen = 2 + u8LenByteCount;

    uint8_t u8Fcs = Mux_CalCRC(arru8CksumData, u8CksumLen);

    pu8Frame[MUX_LEN_IDX+u8LenByteCount+pxInfo->u16Length] = u8Fcs;
    pu8Frame[MUX_LEN_IDX+u8LenByteCount+pxInfo->u16Length+1] = MUX_FLAG;

    *pu16FrameSize = 5 + u8LenByteCount + pxInfo->u16Length;

    return true;
}

/* Statistic function */
#if GSMMUX_CFG_STAT
static void Mux_IncRecvCount(Mux_BufferType *pxBuffer)
{
	pxBuffer->u32RecvCount++;
}

static void Mux_IncDropCount(Mux_BufferType *pxBuffer)
{
	pxBuffer->u32DropCount++;
}
#define	MUX_STAT_INC_RECV_COUNT			Mux_IncRecvCount
#define MUX_STAT_INC_DROP_COUNT			Mux_IncDropCount
#else
#define MUX_STAT_INC_RECV_COUNT(...)
#define MUX_STAT_INC_DROP_COUNT(...)
#endif

/**
 * @brief	Init Mux buffer
 */
static void Mux_BufferInit(Mux_BufferType *pxBuffer)
{
    pxBuffer->pu8EndPtr 	= pxBuffer->arru8Data + MUX_BUFFER_SIZE;
    pxBuffer->pu8ReadPtr 	= pxBuffer->arru8Data;
    pxBuffer->pu8WritePtr 	= pxBuffer->arru8Data;
    pxBuffer->bFlagFound 	= false;

#if GSMMUX_CFG_STAT
    pxBuffer->u32DropCount 	= 0;
    pxBuffer->u32RecvCount 	= 0;
#endif

}

/**
 * @brief	Get mux buffer free
 */
static uint32_t Mux_BufferGetFree(Mux_BufferType *pxBuffer)
{
    return (pxBuffer->pu8ReadPtr > pxBuffer->pu8WritePtr) ?
    		(pxBuffer->pu8ReadPtr - pxBuffer->pu8WritePtr) :
			(MUX_BUFFER_SIZE - (pxBuffer->pu8WritePtr - pxBuffer->pu8ReadPtr));
}

/**
 * @brief	Get mux buffer length
 */
static uint32_t Mux_BufferGetLength(Mux_BufferType *pxBuffer)
{
    return (pxBuffer->pu8ReadPtr > pxBuffer->pu8WritePtr) ?
    		(MUX_BUFFER_SIZE - (pxBuffer->pu8ReadPtr - pxBuffer->pu8WritePtr)) :
			(pxBuffer->pu8WritePtr - pxBuffer->pu8ReadPtr);
}

/**
 * @Brief	Write data into mux buffer
 */
static uint32_t Mux_BufferWrite(Mux_BufferType *pxBuffer, const uint8_t *pu8Data, uint32_t iSize)
{
    int iOffsetToEnd = pxBuffer->pu8EndPtr - pxBuffer->pu8WritePtr;

    iSize = min(iSize, Mux_BufferGetFree(pxBuffer));

    if(iSize > (uint32_t) iOffsetToEnd) {
        memcpy(pxBuffer->pu8WritePtr, pu8Data, iOffsetToEnd);
        memcpy(pxBuffer->arru8Data, pu8Data + iOffsetToEnd, iSize - iOffsetToEnd);
        pxBuffer->pu8WritePtr = pxBuffer->arru8Data + (iSize - iOffsetToEnd);
    }
    else {
        memcpy(pxBuffer->pu8WritePtr, pu8Data, iSize);
        pxBuffer->pu8WritePtr += iSize;
        if(pxBuffer->pu8WritePtr == pxBuffer->pu8EndPtr) {
            pxBuffer->pu8WritePtr = pxBuffer->arru8Data;
        }
    }

    return iSize;
}

/**
 * @brief	Get frame from buffer
 * @param	pBuffer
 * @param	pFrame - pointer to existing frame to store data into
 * @return	true if found frame, false otherwise
 */
static bool Mux_BufferGetFrame(Mux_BufferType *pxBuffer, Mux_FrameType *pxFrame)
{
    uint32_t u32LengthNeeded = 5;
    uint8_t *pu8Data;
    uint8_t u8Fcs = 0xFF;

    while(!pxBuffer->bFlagFound && Mux_BufferGetLength(pxBuffer) > 0) {
        if(*pxBuffer->pu8ReadPtr == MUX_FLAG) {
            pxBuffer->bFlagFound = true;
        }
        BUFFER_INC_PTR(pxBuffer, pxBuffer->pu8ReadPtr);
    }

    if(!pxBuffer->bFlagFound) {
        return NULL;
    }

    /* Skip empty frames */
    while(Mux_BufferGetLength(pxBuffer) > 0 && (*pxBuffer->pu8ReadPtr == MUX_FLAG)) {
        GSMMUX_DBG("E_F\r\n");
    	BUFFER_INC_PTR(pxBuffer, pxBuffer->pu8ReadPtr);
    }

    if(Mux_BufferGetLength(pxBuffer) < u32LengthNeeded) {
        return NULL;
    }

    /* Make copy of readp pointer, read on this pointer does not
     * affect to buffer readp pointer, only when we got complete frame
     * we will update readp pointer
     */
		for(int i = 0; i < Mux_BufferGetLength(pxBuffer); i++)
		GSMMUX_DBG("%x,", pxBuffer->arru8Data[i]);
		GSMMUX_DBG("\r\n");
    pu8Data = pxBuffer->pu8ReadPtr;

    /* Get channel id */
    pxFrame->channel = ((*pu8Data & 0xFC) >> 2);
    u8Fcs = Mux_arru8CrcTable[u8Fcs ^ (*pu8Data)];
    BUFFER_INC_PTR(pxBuffer, pu8Data);

    /* Get control */
    pxFrame->u8Control = *pu8Data;
    u8Fcs = Mux_arru8CrcTable[u8Fcs ^ (*pu8Data)];
    BUFFER_INC_PTR(pxBuffer, pu8Data);

    /* Get data length */
    pxFrame->u16DataLen = (*pu8Data & 0xFE) >> 1;
    u8Fcs = Mux_arru8CrcTable[u8Fcs ^ (*pu8Data)];

    /* Long frame - current spec 7.1.0 states thes kind of frame to be invalid */
    if((*pu8Data & 1) == 0) {
#if MUX_RECEIVE_LONG_FRAME
    	/* If we still want to receive long frame */
    	BUFFER_INC_PTR(pxBuffer, pu8Data);
    	pxFrame->u16DataLen += (*pu8Data * 128);
    	u8Fcs = Mux_arru8CrcTable[u8Fcs & *pu8Data];
    	u32LengthNeeded++;
#else
        /* Update readp & drop */
    	/* Actually UC20 will send frame with max data length is 127 */
        pxBuffer->pu8ReadPtr = pu8Data;
        pxBuffer->bFlagFound = false;
        GSMMUX_DBG("[MUX] DROP LONG FRAME!");
        MUX_STAT_INC_DROP_COUNT(pxBuffer);
        return Mux_BufferGetFrame(pxBuffer, pxFrame);
#endif
    }

    u32LengthNeeded += pxFrame->u16DataLen;
    /* If buffer length is less than real frame length -> return NULL */
    if(Mux_BufferGetLength(pxBuffer) < u32LengthNeeded) {
        return false;
    }

    BUFFER_INC_PTR(pxBuffer, pu8Data);

    /* Extract data */
    if(pxFrame->u16DataLen > 0) {
		int end = pxBuffer->pu8EndPtr - pu8Data;
		if(pxFrame->u16DataLen > end) {
			memcpy(pxFrame->arru8Data, pu8Data, end);
			memcpy(pxFrame->arru8Data + end, pxBuffer->arru8Data, pxFrame->u16DataLen-end);
			pu8Data = pxBuffer->arru8Data + (pxFrame->u16DataLen - end);
		}
		else {
			memcpy(pxFrame->arru8Data, pu8Data, pxFrame->u16DataLen);
			pu8Data += pxFrame->u16DataLen;
			if(pu8Data == pxBuffer->pu8EndPtr) {
				pu8Data = pxBuffer->arru8Data;
			}
		}

		if(FRAME_IS(MUX_CTRL_UI(0), pxFrame)) {
			for(end = 0; end < pxFrame->u16DataLen; end++) {
				u8Fcs = Mux_arru8CrcTable[u8Fcs ^ (pxFrame->arru8Data[end])];
			}
		}
    }

    /* Checksum */
    if(Mux_arru8CrcTable[u8Fcs ^ (*pu8Data)] != 0xCF) {
        pxBuffer->bFlagFound = false;
        pxBuffer->pu8ReadPtr = pu8Data;
        MUX_STAT_INC_DROP_COUNT(pxBuffer);

        return Mux_BufferGetFrame(pxBuffer, pxFrame);
    }
    else {
        BUFFER_INC_PTR(pxBuffer, pu8Data);
        if(*pu8Data != MUX_FLAG) {
            pxBuffer->bFlagFound = false;
            pxBuffer->pu8ReadPtr = pu8Data;
            MUX_STAT_INC_DROP_COUNT(pxBuffer);

            return Mux_BufferGetFrame(pxBuffer, pxFrame);
        }

        BUFFER_INC_PTR(pxBuffer, pu8Data);
    }

    /* Update readp pointer */
    pxBuffer->pu8ReadPtr = pu8Data;
    pxBuffer->bFlagFound = false;

    MUX_STAT_INC_RECV_COUNT(pxBuffer);

    return true;
}

/**************************** GSM MUX data ************************************/
static Mux_BufferType 		Mux_Buffer;
static Mux_FrameType		Mux_Frame;
static Mux_StatusType		Mux_Status = {0};

static xSemaphoreHandle 	Mux_xDataSem;
static xSemaphoreHandle 	Mux_xTransmitLock;
static bool					Mux_bTaskStarted = false;

static uint8_t 				Mux_arru8TmpData[MUX_TMP_READ_BUF_SIZE];
xQueueHandle				gMuxUartQueue;

/**
 * @brief	Notify data received from ISR
 */
void Mux_DataTriggerFromISR(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(Mux_xDataSem, &xHigherPriorityTaskWoken);
}

/**
 * @brief	Notify data received
 */
void Mux_DataTrigger(void)
{
	xSemaphoreGive(Mux_xDataSem);
}

/**
 * @brief	Wait for data received
 */
static void Mux_DataWait(void)
{
	xSemaphoreTake(Mux_xDataSem, portMAX_DELAY);
}

/**
 * @brief	Get right to transmit
 */
void Mux_TransmitLock(void)
{
	xSemaphoreTake(Mux_xTransmitLock, portMAX_DELAY);
}

/**
 * @brief	Release right after transmit
 */
void Mux_TrasmitUnlock(void)
{
	xSemaphoreGive(Mux_xTransmitLock);
}

/**
 * @brief	Read UART from Rx queue; Data is pushed from Mux_OnRxISR
 */
int Mux_UARTRead(uint8_t *pu8Data, int iLen)
{
	uint8_t 	u8Tmp = 0;
	int 		i16Length = 0;

	while(uxQueueMessagesWaiting(gMuxUartQueue)) {
		if(xQueueReceive(gMuxUartQueue, &u8Tmp, 50) != pdPASS) {
			break;
		}

		pu8Data[i16Length++] = u8Tmp;

		if (i16Length >= iLen) {
			break;
        }
	}

	return i16Length;
}

/**
 * @brief	Push rx byte from ISR to queue
 */
bool Mux_UARTInputByteFromISR(char cRxChar)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(xQueueSendToBackFromISR(gMuxUartQueue, &cRxChar, &xHigherPriorityTaskWoken) != pdTRUE) {
    }

    Mux_DataTriggerFromISR();

    return xHigherPriorityTaskWoken;
}

/**
 * @brief	Open DLC
 */
bool Mux_DLCOpen(uint8_t u8Idx)
{
	if(u8Idx >= DLCI_MAX) {
		return false;
	}

	GSMMUX_DBG("Open DLCI %d\r\n", u8Idx);

	uint8_t arru8Buffer[16];
	Mux_FrameInfoType info;
	info.u8Addr 	= MUX_ADDR_EA | MUX_ADDR_CR(1) | MUX_ADDR_DLCI(u8Idx);
	info.u8Control 	= MUX_CTRL_SABM(1);
	info.u16Length 	= 0;

	uint16_t u16Size;

	if(Mux_FrameCreate(&info, arru8Buffer, 16, &u16Size)) {
		return Serial_SendUART(arru8Buffer, u16Size);
	}
	else {
		GSMMUX_DBG("Make frame failed!\r\n");
		return false;
	}
}

/**
 * @brief	Close DLC
 */
bool Mux_DLCClose(uint8_t u8Idx)
{
	if(u8Idx >= DLCI_MAX) {
		return false;
	}

    Mux_FrameInfoType xInfo;

    uint16_t u16Size;
    uint8_t arru8Buffer[16];

    if(u8Idx == DLCI_0) {
        xInfo.u8Addr = MUX_ADDR_EA | MUX_ADDR_CR(1) | MUX_ADDR_DLCI(0);
        xInfo.u8Control = MUX_CTRL_UIH(0);
        uint8_t data[] = {0xC3, 0x01};	/* Close mux also */
        xInfo.pu8Data = data;
        xInfo.u16Length = 2;
    }
    else {
        xInfo.u8Addr = MUX_ADDR_EA | MUX_ADDR_CR(1) | MUX_ADDR_DLCI(u8Idx);
        xInfo.u8Control = MUX_CTRL_DISC(1);
        xInfo.u16Length = 0;
    }

    Mux_FrameCreate(&xInfo, arru8Buffer, 16, &u16Size);

    Serial_SendUART(arru8Buffer, u16Size);

    return true;
}

/**
 * @brief	Set state of DLC
 */
void Mux_DLCSetState(uint8_t idx, bool bState)
{
	if(idx >= DLCI_MAX) {
		return;
	}

	Mux_Status.DLCState[idx] = bState;
}

/**
 * @brief	Get DLC state
 */
bool Mux_DLCGetState(uint8_t idx)
{
	if(idx >= DLCI_MAX) {
		return false;
	}

	return Mux_Status.DLCState[idx];
}


/**
 * @brief	Higher layer call this function to transmit DLC data
 */
bool Mux_DLCSendData(uint8_t u8Idx, uint8_t *pu8Data, int iDataLen,
					 uint8_t *pu8Frame, int i16FrameMaxSize)
{
	if(u8Idx >= DLCI_MAX || Mux_Status.DLCState[u8Idx] == false) {
		return false;
	}

	Mux_FrameInfoType	xInfo;
	xInfo.u8Addr 	= MUX_ADDR_EA | MUX_ADDR_DLCI(u8Idx);
	xInfo.u8Control = MUX_CTRL_UIH(0);
	xInfo.u16Length = iDataLen;
	xInfo.pu8Data 	= pu8Data;

	uint16_t u16Size;

	if(Mux_FrameCreate(&xInfo, pu8Frame, i16FrameMaxSize, &u16Size) != true){
		return false;
	}

	return Serial_SendUART(pu8Frame, u16Size);
}

/**
 * @brief	Send unnumbered ack to request
 */
static void Mux_SendUA(Mux_FrameType *pxFrame)
{
	Mux_FrameInfoType xInfo;
	xInfo.u8Addr 	= MUX_ADDR_CR(0) | MUX_ADDR_EA | MUX_ADDR_DLCI(pxFrame->channel);
	xInfo.u8Control = MUX_CTRL_UA(1);
	xInfo.u16Length = 0;

	uint8_t arru8Frame[16];
	uint16_t u16Size;

	Mux_FrameCreate(&xInfo, arru8Frame, 16, &u16Size);

	Serial_SendUART(arru8Frame, u16Size);
}


/**
 * @brief	Start MUX mode
 */
bool Mux_Start(void)
{
	/* Close Mux if has been already openned */
	for(int idx = DLCI_4; idx >= DLCI_0; idx--) {
		Mux_DLCClose(idx);
		GSMMUX_SLEEP(100);
	}
  Mux_ClearRxBuf();
	GSMMUX_SLEEP(500);
	Serial_SendUART((uint8_t*) MUX_LAUNCH_CMD, strlen(MUX_LAUNCH_CMD));
	GSMMUX_SLEEP(10);
	int iLen = 0, idx = 0;
	volatile int iTry = 0;
	bool bRes = false;
	char arrcResp[32] = {0}, arrcData[32];

	while(iTry++ < 5)
	{
		iLen = Mux_UARTRead((uint8_t*) arrcData, 32);
		if(iLen > 0)
		{
			memcpy(arrcResp + idx, arrcData, iLen);
			idx += iLen;
		}
		else
		{
			if(strstr(arrcResp, "OK") != NULL)
			{
				bRes = true;
				break;
			}
			else
			{
				GSMMUX_SLEEP(500);
			}
		}
	}

	if(bRes == false)
	{
		/* Maybe in CMUX already */
		Mux_DLCOpen(0);
		iTry = 0;
		idx = 0;
		while(iTry++ < 5)
		{
			iLen = Mux_UARTRead((uint8_t*) arrcData, 32);
			if(iLen > 0)
			{
				memcpy(arrcResp + idx, arrcData, iLen);
				idx += iLen;
			}
			else {
				if(idx >= 6)
				{
					uint8_t arru8UA[] = {0xF9, 0x03, 0x73, 0x01, 0xD7, 0xF9};
					if(memcmp(arrcData, arru8UA, 6) == 0)
					{
						bRes = true;
            GSMMUX_DBG("DLCI0 OK\r\n");
            break;
					}
					else
					{
            GSMMUX_DBG("WAIT...");
						bRes = false;
					}
				}
				GSMMUX_SLEEP(100);
			}
		}
	}

	return bRes;
}

/**
 * @brief	Stop MUX mode
 */
bool Mux_Stop(void)
{
	for(int idx = DLCI_MAX - 1; idx >= 0; idx--) {
		if(Mux_Status.DLCState[idx]) {
			Mux_DLCClose(idx);
			GSMMUX_SLEEP(100);
		}
	}

	return true;
}

/**
 * @brief	Clear Mux receive buffer
 */
void Mux_ClearRxBuf(void)
{
	char arrcTmp[32];
	int iLen;

	while(1) {
		iLen = Mux_UARTRead((uint8_t*) arrcTmp, 32);
		if(iLen <= 0) {
			return;
		}
	}
}

/**
 * @brief	Main processing task
 */
static void Mux_MainTask(void const *pvArgument)
{
	(void) 	pvArgument;
	int 	iLen = 0;

	Mux_bTaskStarted = true;

	while(1) {
		Mux_DataWait();
		iLen = Mux_UARTRead(Mux_arru8TmpData, MUX_TMP_READ_BUF_SIZE);

		if(iLen > 0) {
			Mux_BufferWrite(&Mux_Buffer, Mux_arru8TmpData, iLen);

			while(Mux_BufferGetFrame(&Mux_Buffer, &Mux_Frame) != false) {
				Mux_FrameProcess(&Mux_Frame);
			}
		}
	}
}

/**
 * @brief	Init mux call
 */
bool Mux_Init(void)
{
	GSMMUX_DBG("[MUX] Init...\r\n");

	if(Mux_bTaskStarted) {
		GSMMUX_DBG("[MUX] Task has been started already!\r\n");
		return true;
	}

	Mux_BufferInit(&Mux_Buffer);

	Mux_xDataSem = xSemaphoreCreateBinary();

	Mux_xTransmitLock= xSemaphoreCreateMutex();

	gMuxUartQueue = xQueueCreate(128, sizeof(char));
	if(gMuxUartQueue == NULL) {
		GSMMUX_DBG("Queue create failed\r\n");
		return false;
	}


	/* Enable MUX */
	if(Mux_Start() != true) {
		GSMMUX_DBG("Switch CMUX failed!\r\n");
	}
	else {
		GSMMUX_DBG("Switch CMUX OK!\r\n");
	}

	/* Create receive data task */
	osThreadDef(Mux_MainTask, Mux_MainTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadCreate(osThread(Mux_MainTask), NULL);

	/* Wait for task started */
	int iTryCnt = 0;
	while(1) {
		if(Mux_bTaskStarted) {
			break;
		}

		if(iTryCnt++ > 10) {
			return false;
		}

		GSMMUX_SLEEP(100);
	}

	/* Open DLC */
	Mux_DLCOpen(DLCI_0);
	GSMMUX_SLEEP(100);

	for(int idx = DLCI_1; idx <= DLCI_4; idx++) {
		Mux_DLCOpen(idx);
		GSMMUX_SLEEP(100);
	}

	/* Check DLC state */
	GSMMUX_DBG("[MUX] CHANNEL STATE:\r\n");
	for(int idx = 0; idx < DLCI_MAX; idx++) {
		GSMMUX_DBG("CHAN: %d - STATE: %d\r\n", idx, Mux_Status.DLCState[idx]);
		if(Mux_Status.DLCState[idx] == true) {
			Mux_Status.bMuxOn = true;
		}
	}

	return true;
}

#if GSMMUX_CFG_STAT
/**
 * @brief	Get rx & drop counters
 */
void	Mux_GetStat(uint32_t *pu32RxCount, uint32_t *pu32DropCount)
{
	if(pu32RxCount != NULL) {
		*pu32RxCount = Mux_Buffer.u32RecvCount;
	}

	if(pu32DropCount != NULL) {
		*pu32DropCount = Mux_Buffer.u32DropCount;
	}
}
#endif

/**
 * @brief	Handle DLC channel data
 */
static void Mux_DLCDataProcess(uint8_t u8Idx, uint8_t *pu8Data, int iSize)
{
	if(Mux_Status.DLCRecvFn[u8Idx] != NULL) {
		Mux_Status.DLCRecvFn[u8Idx](pu8Data, iSize);
	}
}

static void Mux_DLC0CmdProcess(uint8_t *pu8Data, int iSize)
{
	if(iSize > 0) {
		uint8_t u8CmdType = pu8Data[0];

		/* Command */
		if((u8CmdType & MUX_CMD_CR) == MUX_CMD_CR) {
			switch(u8CmdType & 0xFC) {
			case MUX_CMD_TYPE_MSC:
				GSMMUX_DBG("[MUX] MSC command, TODO!\r\n");
#if 0
				{
					Mux_FrameInfoType xInfo;
					xInfo.u8Addr = MUX_ADDR_CR(1) | MUX_ADDR_DLCI(0) | MUX_ADDR_EA;
					xInfo.u8Control = MUX_CTRL_UIH(1);
					xInfo.pu8Data = pu8Data;
					xInfo.u16Length = iSize;

					uint8_t arru8Tmp[16];
					uint16_t u16Size;

					if(Mux_FrameCreate(&xInfo, arru8Tmp, 16, &u16Size)) {
						GSMMUX_DBG("[MUX] Send MSC back\r\n");
						Serial_SendUART(arru8Tmp, u16Size);
					}
				}
#endif
				break;

			case MUX_CMD_TYPE_CLD:
				GSMMUX_DBG("[MUX] CLD command, TODO!\r\n");
				break;

			case MUX_CMD_TYPE_PSC:
				GSMMUX_DBG("[MUX] PSC command, TODO!\r\n");
				break;

			case MUX_CMD_TYPE_FCON:
				GSMMUX_DBG("[MUX] FCon command, TODO!\r\n");
				break;

			case MUX_CMD_TYPE_FCOFF:
				GSMMUX_DBG("[MUX] FCoff command, TODO!\r\n");
				break;

			default:
				break;
			}
		}
		/* Response */
		else {
			GSMMUX_DBG("[MUX] - Received response to command\r\n");
		}
	}
}

/**
 * @brief	Set DLC data callback
 */
void Mux_DLCSetRecvDataCallback(uint8_t u8Idx, Mux_DLCRecvCallbackType pfn)
{
	if(pfn == NULL || u8Idx >= DLCI_MAX) {
		return;
	}

	Mux_Status.DLCRecvFn[u8Idx] = pfn;
}

/**
 * @brief	Process received frame
 */
static void Mux_FrameProcess(Mux_FrameType *pFrame)
{
	if((FRAME_IS(MUX_CTRL_UI(0), pFrame)) || (FRAME_IS(MUX_CTRL_UIH(0), pFrame))) {
		if(pFrame->channel > 0) {
			/* UI/UIH data channel -> copy to channel data */
			GSMMUX_DBG("[MUX] - C:%d, L:%d\r\n",
					   pFrame->channel, pFrame->u16DataLen);

			Mux_DLCDataProcess(pFrame->channel, pFrame->arru8Data, pFrame->u16DataLen);
		}
		else {
			/* Command from control channel */
			Mux_DLC0CmdProcess(pFrame->arru8Data, pFrame->u16DataLen);
		}
	}
	else {
		switch(pFrame->u8Control & ~MUX_PF) {
		case MUX_CTRL_UA(0):	/* ACK to channel on/off */
			GSMMUX_DBG("[MUX] - RECV UA CHAN %d\r\n", pFrame->channel);
			if(Mux_DLCGetState(pFrame->channel)) {
				Mux_DLCSetState(pFrame->channel, false);
			}
			else {
				Mux_DLCSetState(pFrame->channel, true);
			}
			break;

		case MUX_CTRL_DM(0):	/* Channel on/off already */
			GSMMUX_DBG("[MUX] - RECV DM CHAN %d\r\n", pFrame->channel);
			Mux_DLCSetState(pFrame->channel, false);
			break;

		case MUX_CTRL_DISC(0):	/* Request to disconnect channel, need to ACK */
			GSMMUX_DBG("[MUX] - RECV DISC CHAN %d\r\n", pFrame->channel);
			Mux_SendUA(pFrame);
			Mux_DLCSetState(pFrame->channel, false);
			break;

		case MUX_CTRL_SABM(0):	/* Request to open channel */
			GSMMUX_DBG("[MUX] - RECV SABM CHAN %d\r\n", pFrame->channel);
			break;

		default:
			break;
		}
	}
}

