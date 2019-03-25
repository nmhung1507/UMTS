/*
 * GsmMux.h
 *
 *  Created on: Mar 19, 2019
 *      Author: v.daipv1@vinfast.vn
 */

#ifndef CHARGEPOINT_V2_INC_GSMMUX_H_
#define CHARGEPOINT_V2_INC_GSMMUX_H_

#include <stdint.h>
#include <stdbool.h>
#include "GsmMux_Cfg.h"

#define MUX_FLAG			0xF9

#define MUX_ADDR_EA			1
#define MUX_ADDR_CR(bit)	(bit << 1)
#define MUX_ADDR_DLCI(val)	(val << 2)

#define MUX_CTRL_SABM(PF)   (0x2F | (PF << 4))
#define MUX_CTRL_UA(PF)     (0x63 | (PF << 4))
#define MUX_CTRL_DM(PF)     (0x0F | (PF << 4))
#define MUX_CTRL_DISC(PF)   (0x43 | (PF << 4))
#define MUX_CTRL_UIH(PF)    (0xEF | (PF << 4))
#define MUX_CTRL_UI(PF)     (0x03 | (PF << 4))

#define MUX_FLAG_IDX        0
#define MUX_ADDR_IDX        1
#define MUX_CTRL_IDX        2
#define MUX_LEN_IDX         3

#define MUX_PF  16

#define MUX_CMD_EA			1
#define MUX_CMD_CR			(1 << 1)

#define MUX_CMD_TYPE_MSC	0xE0	/* Modem status command */
#define MUX_CMD_TYPE_CLD	0xC0	/* Mux close down command */
#define MUX_CMD_TYPE_PSC	0x40	/* Power saving command */
#define MUX_CMD_TYPE_FCON	0xA0
#define MUX_CMD_TYPE_FCOFF	0x60

#define FRAME_IS(type, frame)   	((frame->u8Control & ~MUX_PF) == type)

#define BUFFER_INC_PTR(buf, ptr)    {ptr++; if(ptr == buf->pu8EndPtr) ptr = buf->arru8Data; }

#define MUX_LAUNCH_CMD      	"AT+CMUX=0\r\n"

#ifndef GSMMUX_CFG_BUFF_SIZE
#define GSMMUX_CFG_BUFF_SIZE	256
#endif

#define MUX_BUFFER_SIZE     	GSMMUX_CFG_BUFF_SIZE

#define MUX_TMP_READ_BUF_SIZE	128

#define MUX_RECEIVE_LONG_FRAME	0

#if MUX_RECEIVE_LONG_FRAME
#define MUX_FRAME_DATA_SIZE 	256
#else
#define MUX_FRAME_DATA_SIZE 	128
#endif

#ifndef GSMMUX_CFG_STAT
#define GSMMUX_CFG_STAT			0
#endif

typedef struct {
    uint8_t     u8Addr;
    uint8_t     u8Control;
    uint16_t    u16Length;
    uint8_t     *pu8Data;
} Mux_FrameInfoType;

typedef struct {
    uint8_t     channel;
    uint8_t     u8Control;
    uint16_t    u16DataLen;
    uint8_t     arru8Data[MUX_FRAME_DATA_SIZE];
} Mux_FrameType;

typedef struct {
    uint8_t     arru8Data[MUX_BUFFER_SIZE];
    uint8_t     *pu8ReadPtr;
    uint8_t     *pu8WritePtr;
    uint8_t     *pu8EndPtr;
    bool        bFlagFound;
#if GSMMUX_CFG_STAT
    uint32_t	u32RecvCount;
    uint32_t	u32DropCount;
#endif
} Mux_BufferType;

typedef enum {
	DLCI_0 = 0,
	DLCI_1,
	DLCI_2,
	DLCI_3,
	DLCI_4,
	DLCI_MAX
} Mux_DLCType;

typedef bool	(*Mux_DLCRecvCallbackType)(uint8_t *pu8Data, int iSize);

typedef struct {
    bool    						bMuxOn;
    bool    						DLCState[DLCI_MAX];
    Mux_DLCRecvCallbackType			DLCRecvFn[DLCI_MAX];
} Mux_StatusType;

extern xQueueHandle gMuxUartQueue;

bool		Mux_Init(void);
bool 		Mux_Start(void);
bool 		Mux_Stop(void);
void 		Mux_ClearRxBuf(void);

void		Mux_DLCSetState(uint8_t idx, bool bState);
bool		Mux_DLCGetState(uint8_t idx);
bool 		Mux_DLCOpen(uint8_t u8Idx);
bool 		Mux_DLCClose(uint8_t u8Idx);
void 		Mux_DLCClearDataQueue(uint8_t u8Idx);
bool 		Mux_DLCSendData(uint8_t u8Idx, uint8_t *pu8Data, int iDataLen,
					 	 	uint8_t *pu8Frame, int i16FrameMaxSize);
void		Mux_DLCSetRecvDataCallback(uint8_t u8Idx, Mux_DLCRecvCallbackType pfn);

int 		Mux_UARTRead(uint8_t *pu8Data, int iLen);

void 		Mux_TransmitLock(void);
void 		Mux_TrasmitUnlock(void);
void 		Mux_OnRxISR(void);
bool 		Mux_UARTInputByteFromISR(char cRxChar);

void 		Mux_DataTriggerFromISR(void);
void 		Mux_DataTrigger(void);
void 		Mux_DataWait(void);

#if GSMMUX_CFG_STAT
void		Mux_GetStat(uint32_t *pu32RxCount, uint32_t *pu32DropCount);
#endif

#endif /* CHARGEPOINT_V2_INC_GSMMUX_H_ */
