/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _UMTS_H_
#define _UMTS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/
typedef struct
{
  char        *pcCmd;                    /* Command */
  uint16_t    u16CmdSize;                /* Command size */
  char        *pcCmdResponseOnOK;        /* Expected response */
  uint16_t    i16TimeoutMs;            /* Wait for response timeout */
  uint16_t    i16DelayMs;                /* Delay before send next command */
  bool        bSkip;                    /* Skip this command */
} Umts_CmdType;

/* Private defines -----------------------------------------------------------*/
#define UMTS_OK_STR  "OK"
#define UMTS_ERROR_STR "ERROR"
#define UMTS_NEWLINE_STR  "\r\n"

#define GSM_DATA_RECEIVED_EVENT        0x1U

#define UMTS_CMD_AT                  "AT\r\n"
#define UMTS_CMD_AT_TIMEOUT          1000
#define UMTS_CMD_AT_DELAY_NEXT       500

#define UMTS_CMD_CNMI                "AT+CNMI=0,0,0,0,0\r\n"
#define UMTS_CMD_CNMI_TIMEOUT        1000
#define UMTS_CMD_CNMI_DELAY_NEXT     500

#define UMTS_CMD_RESET               "ATZ\r\n"
#define UMTS_CMD_RESET_TIMEOUT       500
#define UMTS_CMD_RESET_DELAY_NEXT    500

#define UMTS_CMD_RFON                "AT+CFUN=1\r\n"
#define UMTS_CMD_RFON_TIMEOUT        5000
#define UMTS_CMD_RFON_DELAY_NEXT     500

#define UMTS_CMD_RFOFF               "AT+CFUN=4\r\n"
#define UMTS_CMD_RFOFF_TIMEOUT       5000

#define UMTS_CMD_GET_CFUN            "AT+CFUN?\r\n"
#define UMTS_CMD_GET_CFUN_RESP_OK    "+CFUN: 1"
#define UMTS_CMD_GET_CFUN_TIMEOUT    2000

#define UMTS_CMD_ATH                 "ATH\r\n"
#define UMTS_CMD_ATH_NO_CARRIER_RESP "NO CARRIER"
#define UMTS_CMD_ATH_TIMEOUT         3000

#define UMTS_CMD_ECHO_OFF            "ATE0\r\n"
#define UMTS_CMD_ECHO_OFF_TIMEOUT    500
#define UMTS_CMD_ECHO_OFF_DELAY_NEXT 500

#define UMTS_CMD_CPIN                "AT+CPIN?\r\n"
#define UMTS_CMD_PIN_RESP_OK         "CPIN: READY"
#define UMTS_CMD_CPIN_TIMEOUT        10000
#define UMTS_CMD_CPIN_DELAY_NEXT     500

#define UMTS_CMD_CCID                "AT+CCID\r\n"
#define UMTS_CMD_CCID_TIMEOUT        1000
#define UMTS_CMD_CCID_DELAY_NEXT     500

#define UMTS_CMD_CIMI                "AT+CIMI\r\n"
#define UMTS_CMD_CIMI_TIMEOUT        1000
#define UMTS_CMD_CIMI_DELAY_NEXT     500

#define UMTS_CMD_CREG                "AT+CREG?\r\n"
#define UMTS_CMD_REG_RESP_OK         "CREG: 0,1"
#define UMTS_CMD_CREG_TIMEOUT        4000
#define UMTS_CMD_CREG_DELAY_NEXT     100

#define UMTS_CMD_CONNECT             "AT+CGDATA=\"PPP\",1\r\n"
#define UMTS_CMD_CONNECT_RESP_OK     "CONNECT"
#define UMTS_CMD_CONNECT_TIMEOUT     10000
#define UMTS_CMD_CONNECT_DELAY_NEXT  500

#define UMTS_CMD_CMGF                "AT+CMGF=1\r\n"
#define UMTS_CMD_CMGF_TIMEOUT        1000

#define UMTS_CMD_DEL_SMS_TIMEOUT     5000
#define UMTS_CMD_CGML_TIMEOUT        2000

/* period in second will be assemble to the tail */
#define UMTS_CMD_GPS_PERIOD          "AT+QGPS=1,1,0,0," 
#define UMTS_CMD_GPS_END             "AT+QGPSEND\r\n"
#define UMTS_CMD_QGPS_TIMEOUT        2000

#define UMTS_CMD_GPS_CFG_OUTPORT_UARTDEBUG   "AT+QGPSCFG=\"outport\",\"uartdebug\"\r\n"
#define UMTS_CMD_GPS_CFG_OUTPORT_CMUX3       "AT+QGPSCFG=\"outport\",\"cmux3\"\r\n"
#define UMTS_CMD_GPS_CFG_OUTPUT_RMC          "AT+QGPSCFG=\"gpsnmeatype\",2\r\n"
#define UMTS_CMD_QGPSCFG_TIMEOUT             2000

#define UMTS_CMD_MAX_FAIL_COUNT    5

#define NULL_CHAR  '\0'
#define SPACE_CHAR 0x20
#define DEL_CHAR   0x7F
#define DOT_CHAR  '.'

#define UMTS_RESPONSE_BUF_LEN  128
#define UMTS_CCID_BUF_LEN  25
#define UMTS_CIMI_BUF_LEN  20
#define UMTS_BUF_LEN_64  64

#define UMTS_SEND_MAX_RETRY  100
#define UMTS_SLEEP_ON_CMD_FAIL  1000
#define UMTS_SLEEP_10MS            10

/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
/**
 * @brief    Send AT command & wait for response
 * @param    pcCmd - pointer to send command 
 * @param    pcResp - pointer to first expected response to compare (may be substring of response) 
 * @param    pcResp2 - pointer to second expected response to compare
 * @param    iCmdSize - size of send command 
 * @param    iTimeout - timeout
 * @param    pcResponse - point to response array.
 * @param    iSize: Max size of pcResponse.
 * @return   0 if not received any expected response, 
 *           1 if received first response
 *           2 for second response
 */
int Umts_SendAT(const char * pcCmd, const char *pcResp, const char * pcResp2, 
                                  int iCmdSize, int iTimeout, char *pcResponse, int iSize);


#ifdef __cplusplus
}
#endif

#endif /* _UMTS_H_ */

/******************************END OF FILE*************************************/
