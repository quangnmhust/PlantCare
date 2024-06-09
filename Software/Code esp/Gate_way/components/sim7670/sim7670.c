#include <stdio.h>
#include "sim7670.h"

void SendATCommand()
{
	uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)SIMCOM_ATCommand.CMD, strlen(SIMCOM_ATCommand.CMD));
	ESP_LOGI(TAG_ATCommand,"Send:%s\n",SIMCOM_ATCommand.CMD);
	ESP_LOGI(TAG_ATCommand,"Packet:\n-ExpectResponseFromATC:%s\n-RetryCountATC:%d\n-TimeoutATC:%ld\n-CurrentTimeoutATC:%ld",SIMCOM_ATCommand.ExpectResponseFromATC
			,SIMCOM_ATCommand.RetryCountATC,SIMCOM_ATCommand.TimeoutATC,SIMCOM_ATCommand.CurrentTimeoutATC);
	Flag_Wait_Exit = false;
}
void ATC_SendATCommand(const char * Command, char *ExpectResponse, uint32_t timeout, uint8_t RetryCount, SIMCOM_SendATCallBack_t Callback){
	strcpy(SIMCOM_ATCommand.CMD, Command);
	SIMCOM_ATCommand.lenCMD = strlen(SIMCOM_ATCommand.CMD);
	strcpy(SIMCOM_ATCommand.ExpectResponseFromATC, ExpectResponse);
	SIMCOM_ATCommand.RetryCountATC = RetryCount;
	SIMCOM_ATCommand.SendATCallBack = Callback;
	SIMCOM_ATCommand.TimeoutATC = timeout;
	SIMCOM_ATCommand.CurrentTimeoutATC = 0;
	SendATCommand();
}


void RetrySendATC(){
	SendATCommand();
}


void ATResponse_callback(SIMCOM_ResponseEvent_t event, void *ResponseBuffer){
	AT_RX_event  = event;
	if(event == EVENT_OK){
		ESP_LOGI(TAG_ATCommand, "Device is ready to use\r\n");
		Flag_Wait_Exit = true;
		Flag_Device_Ready = true;
	}
	else if(event == EVENT_TIMEOUT){
		ESP_LOGE(TAG_ATCommand, "Timeout, Device is not ready\r\n");
		Flag_Wait_Exit = true;
	}
	else if(event == EVENT_ERROR){
		ESP_LOGE(TAG_ATCommand, "AT check Error \r\n");
		Flag_Wait_Exit = true;
	}
}


void  WaitandExitLoop(bool *Flag)
{
	while(1)
	{
		if(*Flag == true)
		{
			*Flag = false;
			break;
		}
		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
}


bool check_SIMA7670C(void)
{
	ATC_SendATCommand("AT\r\n", "OK", 2000, 4, ATResponse_callback);
	WaitandExitLoop(&Flag_Wait_Exit);
	if(AT_RX_event == EVENT_OK){
		return true;
	}else{
		return false;
	}
}


void SendSMSMessage(char* phoneNumber, char* message){
    char atCommand[256];

    // Set SMS format to text mode
    ATC_SendATCommand("AT+CMGF=1\r\n", "OK", 2000, 4, ATResponse_callback);
    WaitandExitLoop(&Flag_Wait_Exit);

    // Set the phone number
    sprintf(atCommand, "AT+CMGS=\"%s\"\r\n", phoneNumber);
    ATC_SendATCommand(atCommand, ">", 2000, 4, ATResponse_callback);
    WaitandExitLoop(&Flag_Wait_Exit);

    // Set the message
    sprintf(atCommand, "%s\x1A", message); // End of message character (ASCII 26)
    ATC_SendATCommand(atCommand, "OK", 2000, 4, ATResponse_callback);
    WaitandExitLoop(&Flag_Wait_Exit);
}

void Connect_Server(void)
{
	if(check_SIMA7670C()){
		Flag_Device_Ready = true;
		ESP_LOGI(SIM_TAG,"Connected to SIMA7670C\r\n");
		ATC_SendATCommand("AT+HTTPINIT\r\n", "OK", 2000, 2,ATResponse_callback);
		WaitandExitLoop(&Flag_Wait_Exit);
		sprintf(AT_BUFFER ,"AT+HTTPPARA=\"URL\",\"http://sanslab.ddns.net:5000/api/data/sendData2?\"\r\n");
		ATC_SendATCommand(AT_BUFFER, "OK", 10000, 3, ATResponse_callback);
		sprintf(AT_BUFFER1 ,"AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n");
		ATC_SendATCommand(AT_BUFFER1, "OK", 10000, 3, ATResponse_callback);		
		WaitandExitLoop(&Flag_Wait_Exit);
		
    cJSON *root = cJSON_CreateObject();
    cJSON *devices = cJSON_CreateArray();

    cJSON_AddItemToObject(root, "gateway_API", cJSON_CreateString("QDEDw9snWnmS03W7wY"));
    cJSON_AddItemToObject(root, "devices", devices);

    cJSON *device1 = cJSON_CreateObject();
    cJSON *sensorData1 = cJSON_CreateObject();
    cJSON_AddItemToObject(device1, "device_API", cJSON_CreateString("QDEDw9snWnmS03W7wYnlkLddeg"));
    cJSON_AddItemToObject(device1, "sensorData", sensorData1);
    cJSON_AddItemToObject(sensorData1, "7W85OlSssnfaoLwgczSK4zb5EMgWG68o4b", cJSON_CreateNumber(1.12));
    cJSON_AddItemToObject(sensorData1, "7W85OlSssnfaoLwgczSK4zb5EMQMeonYig", cJSON_CreateNumber(2.12));
    cJSON_AddItemToObject(sensorData1, "7W85OlSssnfaoLwgczSK4zb5EMLSw1AYaMy", cJSON_CreateNumber(3.12));
    cJSON_AddItemToObject(sensorData1, "7W85OlSssnfaoLwgczSK4zb5EMvQHcZFas", cJSON_CreateNumber(4.12));
    cJSON_AddItemToArray(devices, device1);
    
	    cJSON *root1 = cJSON_CreateObject();
	    cJSON *devices1 = cJSON_CreateArray();
	    cJSON_AddItemToObject(root1, "gateway_API", cJSON_CreateString("QDEDw9snWnmS03W7wY"));
	    cJSON_AddItemToObject(root1, "devices", devices1);	    
	    
	    cJSON *device2 = cJSON_CreateObject();
	    cJSON *sensorData2 = cJSON_CreateObject();
	    cJSON_AddItemToObject(device2, "device_API", cJSON_CreateString("QDEDw9snWnmS03W7wYSG6vi7mo"));
	    cJSON_AddItemToObject(device2, "sensorData", sensorData2);
	    cJSON_AddItemToObject(sensorData2, "QDEDw9snWnmS03W7wYSG6vi7moRt5JKdBE", cJSON_CreateNumber(11.12));
	    cJSON_AddItemToObject(sensorData2, "QDEDw9snWnmS03W7wYSG6vi7moZUniGrV7", cJSON_CreateNumber(6.12));
	    cJSON_AddItemToObject(sensorData2, "QDEDw9snWnmS03W7wYSG6vi7moMQEq8ztO", cJSON_CreateNumber(7.12));
	    cJSON_AddItemToObject(sensorData2, "QDEDw9snWnmS03W7wYSG6vi7moCtligoVx", cJSON_CreateNumber(8.12));
	    cJSON_AddItemToArray(devices1, device2);

	    char *json_string = cJSON_Print(root);
	    
	    char *json_string1 = cJSON_Print(root1);	
	    
	        		       
	        char content_length[16];
	        sprintf(content_length, "%d", strlen(json_string));
		  // Construct AT command string
		char atCommandStr[50];
		strcpy(atCommandStr, "AT+HTTPDATA=");
		strcat(atCommandStr, content_length);
		strcat(atCommandStr, ",1000\r\n");

		ATC_SendATCommand(atCommandStr, "OK", 2000, 0, NULL);
		

		vTaskDelay(1000/portTICK_PERIOD_MS);

		ATC_SendATCommand(json_string, "OK", 5000, 0, ATResponse_callback);
		WaitandExitLoop(&Flag_Wait_Exit);
		
		ATC_SendATCommand("AT+HTTPACTION=1\r\n","OK" ,2000, 2, ATResponse_callback);
		WaitandExitLoop(&Flag_Wait_Exit);
				
	        char content_length1[16];
	        sprintf(content_length1, "%d", strlen(json_string1));
		  // Construct AT command string
		char atCommandStr1[50];
		strcpy(atCommandStr1, "AT+HTTPDATA=");
		strcat(atCommandStr1, content_length1);
		strcat(atCommandStr1, ",1000\r\n");

		ATC_SendATCommand(atCommandStr1, "OK", 2000, 0, NULL);		
		//WaitandExitLoop(&Flag_Wait_Exit);
		vTaskDelay(1000/portTICK_PERIOD_MS);
				
		ATC_SendATCommand(json_string1, "OK", 5000, 0, ATResponse_callback);
		WaitandExitLoop(&Flag_Wait_Exit);
										
		ATC_SendATCommand("AT+HTTPACTION=1\r\n","OK" ,2000, 2, ATResponse_callback);
		WaitandExitLoop(&Flag_Wait_Exit);
		ATC_SendATCommand("AT+HTTPHEAD\r\n","OK" ,2000, 2, ATResponse_callback);
		WaitandExitLoop(&Flag_Wait_Exit);
		ATC_SendATCommand("AT+HTTPREAD=0,500\r\n","OK" ,2000, 2, ATResponse_callback);
		WaitandExitLoop(&Flag_Wait_Exit);			
		ATC_SendATCommand("AT+HTTPTERM\r\n","OK" ,2000, 2, ATResponse_callback);
		WaitandExitLoop(&Flag_Wait_Exit);							
		cJSON_Delete(root);
		free(json_string);                
	}
}

void Timeout_task(void *arg)
{
	while (1)
	{
		if((SIMCOM_ATCommand.TimeoutATC > 0) && (SIMCOM_ATCommand.CurrentTimeoutATC < SIMCOM_ATCommand.TimeoutATC))
		{
			SIMCOM_ATCommand.CurrentTimeoutATC += TIMER_ATC_PERIOD;
			if(SIMCOM_ATCommand.CurrentTimeoutATC >= SIMCOM_ATCommand.TimeoutATC)
			{
				SIMCOM_ATCommand.CurrentTimeoutATC -= SIMCOM_ATCommand.TimeoutATC;
				if(SIMCOM_ATCommand.RetryCountATC > 0)
				{
					ESP_LOGI(SIM_TAG,"retry count %d",SIMCOM_ATCommand.RetryCountATC-1);
					SIMCOM_ATCommand.RetryCountATC--;
					RetrySendATC();
				}
				else
				{
					if(SIMCOM_ATCommand.SendATCallBack != NULL)
					{
						printf("Time out!\n"); 

						SIMCOM_ATCommand.TimeoutATC = 0;
						SIMCOM_ATCommand.SendATCallBack(EVENT_TIMEOUT, "@@@");
						ATC_Sent_TimeOut = 1;
					}
				}
			}
		}
		vTaskDelay(TIMER_ATC_PERIOD / portTICK_PERIOD_MS);
	}

}