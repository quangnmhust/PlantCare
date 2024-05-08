#ifndef LIB_COMMON_H_
#define LIB_COMMON_H_

#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define GET_DATA_PERIOD 5
#define SEND_DATA_PERIOD 1

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_DISCONNECTED_BIT BIT1
#define MQTT_CONNECTED_BIT BIT2
#define MQTT_DISCONNECTED_BIT BIT3
#define HTTP_CONNECTED_BIT BIT4
#define HTTP_DISCONNECTED_BIT BIT5

#define RS485_PORT UART_NUM_2
#define RS485_RX 16
#define RS485_TX 17

#define SDA_PIN 21
#define SCL_PIN 22
#define ADDR BH1750_ADDR_LO

#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   64
#define soil_Air 2000
#define soil_Water 4159

uint8_t MAC_address[6];

void program_init(void);

void sht_sensor_task(void *pvParameters);
// void soil_sensor_task(void *pvParameters);
void lux_sensor_task(void *pvParameters);
void adc_sensor_task(void *pvParameters);
void rtc_sensor(void *pvParameters);
void get_data_sensor(void *pvParameters);
void send_data_http(void *pvParameters);
void send_data_mqtt(void *pvParameters);

void wifi_connection(void);
void mqtt_app_start(void);

float map(uint32_t x, int in_min, int in_max, int out_min, int out_max);

typedef struct
{
	uint16_t time_year;
	uint8_t time_month;
	uint8_t time_day;
	uint8_t time_hour;
	uint8_t time_min;
	uint8_t time_sec;
} data_time;

typedef struct Data_manager
{
    data_time time_data;

    float Soil_temp;
    float Soil_humi;
    float Env_temp;
    float Env_Humi;
    uint16_t Env_Lux;
} Data_t;

typedef struct Device_manager
{
    bool soil_status;
    bool sht_status;
    bool lux_status;

    bool rtc_status;
    bool sd_status;
    bool rf_status;
} Device_t;
#endif
