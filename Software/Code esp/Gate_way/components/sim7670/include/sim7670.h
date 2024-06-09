#ifndef __SIM7670C_H__
#define __SIM7670C_H__

#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdbool.h>
#include<stdlib.h>
#include <math.h>
#include <string.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "general.h"

const char *TAG_UART = "SIM";

#define AT_BUFFER_SZ 1024

#define ECHO_TEST_TXD  17
#define ECHO_TEST_RXD  18
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)
#define ECHO_UART_PORT_NUM      2
#define ECHO_UART_BAUD_RATE     115200

#define uart_rx_task_STACK_SIZE    2048

#define TIMER_ATC_PERIOD 50

#define DAM_BUF_TX 512
#define DAM_BUF_RX 2049
#define MAX_RETRY	10
#define BUF_SIZE (2048)

#define SERVER "sanslab.ddns.net"
#define PORT "5000"

extern  uint8_t ATC_Sent_TimeOut;
extern bool Flag_Connected_Server ;
extern bool Flag_Wait_Exit;
extern bool Flag_Device_Ready;

TaskHandle_t check_timeout_task_handle;

typedef enum
{
    EVENT_OK = 0,
    EVENT_TIMEOUT,
    EVENT_ERROR,
} SIMCOM_ResponseEvent_t;

typedef void (*SIMCOM_SendATCallBack_t)(SIMCOM_ResponseEvent_t event, void *ResponseBuffer);
extern SIMCOM_ResponseEvent_t AT_RX_event;
typedef struct {
	char CMD[DAM_BUF_TX];
	uint32_t lenCMD;
	char ExpectResponseFromATC[20];
	uint32_t TimeoutATC;
	uint32_t CurrentTimeoutATC;
	uint8_t RetryCountATC;
	SIMCOM_SendATCallBack_t SendATCallBack;
}ATCommand_t;
extern  ATCommand_t SIMCOM_ATCommand;
extern char AT_BUFFER[AT_BUFFER_SZ];
extern char AT_BUFFER1[AT_BUFFER_SZ];
extern char *SIM_TAG ;

typedef struct{
	char * API_key;
	char *Server;
	char *Port;
}Server;


extern Server server_sanslab;
bool Flag_Connected_Server = false;
uint8_t ATC_Sent_TimeOut = 0;
char AT_BUFFER[AT_BUFFER_SZ] = "";
char AT_BUFFER1[AT_BUFFER_SZ] = "";
char AT_BUFFER2[AT_BUFFER_SZ] = "";
SIMCOM_ResponseEvent_t AT_RX_event;
Server server_sanslab ={
		.Server = SERVER,
		.Port = PORT,
};
bool Flag_Wait_Exit = false ; // flag for wait response from sim or Timeout and exit loop
bool Flag_Device_Ready = false ; // Flag for SIM ready to use, False: device not connect, check by use AT\r\n command
char *TAG_ATCommand= "AT COMMAND";
ATCommand_t SIMCOM_ATCommand;

char *SIM_TAG = "UART SIM";

void  WaitandExitLoop(bool *Flag);
bool check_SIMA7670C(void);
void Timeout_task(void *arg);
void Connect_Server(void );
void SendATCommand(void);
void RetrySendATC();
void ATC_SendATCommand(const char * Command, char * ExectResponse, uint32_t Timeout, uint8_t RetryCount, SIMCOM_SendATCallBack_t Callback);
void ATResponse_callback(SIMCOM_ResponseEvent_t event, void *ResponseBuffer);

#endif