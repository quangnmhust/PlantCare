#include <stdio.h>
#include "sdkconfig.h"
#include "nvs_flash.h"
#include <time.h>
#include <sys/time.h>

#include "esp_system.h"
#include "mqtt_client.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_adc_cal.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "driver/uart.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/event_groups.h"


#include "common.h"
#include "i2cdev.h"
#include <bh1750.h>
#include <sht4x.h>
#include <ds3231.h>

#define SSID CONFIG_SSID
#define PASS CONFIG_PASSWORD
#define API_KEY CONFIG_API_KEY
#define WEB_SERVER  CONFIG_WEB_SERVER
#define WEB_PORT  CONFIG_WEB_PORT       

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_7;
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;


TaskHandle_t soil_sensor_handle = NULL;
TaskHandle_t sht_sensor_handle = NULL;
TaskHandle_t lux_sensor_handle = NULL;
TaskHandle_t adc_sensor_handle = NULL;
TaskHandle_t rtc_sensor_handle = NULL;

TaskHandle_t get_data_handle = NULL;
TaskHandle_t send_http_handle = NULL;
TaskHandle_t send_mqtt_handle = NULL;

QueueHandle_t dataHTTP_queue;
QueueHandle_t dataMQTT_queue;

SemaphoreHandle_t I2C_mutex = NULL;
SemaphoreHandle_t send_data_mutex = NULL;

static EventGroupHandle_t wifi_event_group;
static EventGroupHandle_t http_event_group;
static EventGroupHandle_t mqtt_event_group;
esp_mqtt_client_handle_t mqttClient_handle = NULL;

volatile Data_t Data;
volatile Device_t Device_status = {false, false, false, false, false, false};
char data_string[256];

static sht4x_t sht41;
i2c_dev_t bh1750;
i2c_dev_t ds3231;

const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
struct addrinfo *res;
struct in_addr *addr;

int s, r;

char REQUEST[512];
char recv_buf[512];

//I2C Init
void I2C_init(void);
static void check_efuse(void);
static void print_char_val_type(esp_adc_cal_value_t val_type);
void ADC_Init(void);
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void mqtt_event_handler (void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
/*---------------------------------------------------------*/
void app_main(void){

    //Init
    program_init();

    /*Task*/

    //ADC Task
    // xTaskCreatePinnedToCore(adc_sensor_task, "adc_sensor_task", 2048*2, NULL, 5, &adc_sensor_handle, tskNO_AFFINITY);

    // I2C Task
    // SHT
    // xTaskCreatePinnedToCore(sht_sensor_task, "sht_sensor_task", 2048*2, NULL, 5, &sht_sensor_handle, tskNO_AFFINITY);
    // BH1750
    xTaskCreatePinnedToCore(lux_sensor_task, "lux_sensor_task", 2048*2, NULL, 5, &lux_sensor_handle, tskNO_AFFINITY);
    //ds3231
    // xTaskCreatePinnedToCore(rtc_sensor, "rtc_sensor", 2048*2, NULL, 5, &rtc_sensor_handle, tskNO_AFFINITY);

    //Onewire Task
    // xTaskCreatePinnedToCore(soil_sensor_task, "soil_sensor_task", 2048*2, NULL, 5, &soil_sensor_handle, tskNO_AFFINITY);

    //Get and send data
    // xTaskCreatePinnedToCore(get_data_sensor, "get_data_sensor", 2048*2, NULL, 7, &get_data_handle, tskNO_AFFINITY);
    // xTaskCreatePinnedToCore(send_data_http, "send_data_http", 2048*2, NULL, 8, &send_http_handle, tskNO_AFFINITY);
    
}
/*----------------------------------------------------------*/


void program_init(void){
    //Semaphore init
    I2C_mutex = xSemaphoreCreateMutex();
    send_data_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(__func__, "Create Semaphore success.");

    //Queue init
    dataHTTP_queue = xQueueCreate(20, sizeof(Data_t));
    dataMQTT_queue = xQueueCreate(20, sizeof(Data_t));
	ESP_LOGI(__func__, "Create Queue success.");

    //Wifi and MQTT connection
    // wifi_connection();
    
    //ADC init
    // ADC_Init();

    //I2C init
    I2C_init();

}

void rtc_sensor(void *pvParameters){
    if(ds3231_init_desc(&ds3231, 0, SDA_PIN, SCL_PIN) ==  ESP_OK){
        Device_status.rtc_status = true;
    }
    TickType_t last_wakeup = xTaskGetTickCount();
    while(1)
    {
        if(xSemaphoreTake(I2C_mutex, portMAX_DELAY) == pdTRUE){
            struct tm time_c;

            if (ds3231_get_time(&ds3231, &time_c) != ESP_OK)
            {
            printf("Could not get time\n");
            continue;
            }

            Data.time_data.time_year=time_c.tm_year+1900;
            Data.time_data.time_month=time_c.tm_mon+1;
            Data.time_data.time_day=time_c.tm_mday;
            Data.time_data.time_hour=time_c.tm_hour;
            Data.time_data.time_min=time_c.tm_min;
            Data.time_data.time_sec=time_c.tm_sec;

            xSemaphoreGive(I2C_mutex);
        }
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(1000));
    }
}

void sht_sensor_task(void *pvParameters){
    if(sht4x_init_desc(&sht41, 0, SDA_PIN, SCL_PIN) == ESP_OK){
        Device_status.sht_status = true;
    }
    ESP_ERROR_CHECK(sht4x_init(&sht41));

    float temperature;
    float humidity;

    TickType_t last_wakeup = xTaskGetTickCount();

    while (1)
    {   
        vTaskDelay(2000/portTICK_RATE_MS);
        // perform one measurement and do something with the results
        ESP_ERROR_CHECK(sht4x_measure(&sht41, &temperature, &humidity));
        // humidity+=8;
        printf("sht4x Sensor: %.2f °C, %.2f %%\n", temperature, humidity);
        Data.Env_temp = temperature;
        Data.Env_Humi = humidity;
        // wait until 5 seconds are over
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(GET_DATA_PERIOD * 60000));
    }
    
}
// void soil_sensor_task(void *pvParameters){
//     while (1)
//     {
//         /* code */
//     }
    
// }

void lux_sensor_task(void *pvParameters){
    
    if(bh1750_init_desc(&bh1750, ADDR, 0, SDA_PIN, SCL_PIN) == ESP_OK){
        Device_status.lux_status = true;
    }
    ESP_ERROR_CHECK(bh1750_setup(&bh1750, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH));
    uint16_t lux;

    TickType_t last_wakeup = xTaskGetTickCount();

    while (1)
    {   
        vTaskDelay(2000/portTICK_RATE_MS);
        if(xSemaphoreTake(I2C_mutex, portMAX_DELAY) == pdTRUE){

            if (bh1750_read(&bh1750, &lux) != ESP_OK)
                printf("Could not read lux data\n");
            else
                printf("Lux: %d\n", lux);
            
            Data.Env_Lux = lux;
            xSemaphoreGive(I2C_mutex);
        }
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(GET_DATA_PERIOD * 60000));
    }
}

void adc_sensor_task(void *pvParameters){
    uint32_t adc_reading = 0;
    TickType_t last_wakeup = xTaskGetTickCount();
    while (1)
    {
        vTaskDelay(2000/portTICK_RATE_MS);
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, width, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;

        float soil_percent = 100.0- map(adc_reading, soil_Air, soil_Water, 0, 100);
        Data.Soil_humi = soil_percent;

        printf("Soil percent: %.2f, %d\n", Data.Soil_humi, adc_reading);

        // vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(GET_DATA_PERIOD * 60000));
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(10000));
    }
}   

void get_data_sensor(void *pvParameters){
    Data_t data_temp = {};
    Device_t device_temp = {};
    TickType_t last_wakeup = xTaskGetTickCount();
    while(1){
        device_temp = Device_status;
        ESP_LOGI(__func__, "SHT: %d, BH1750: %d, ADC: %d, RTC: %d", device_temp.sht_status, device_temp.lux_status, device_temp.soil_status, device_temp.rtc_status);
        vTaskDelay(6000/portTICK_RATE_MS);
        data_temp = Data;
        memset(data_string, 0, 256);
        sprintf(data_string, "{\n\t\"Time_real_Date\":\"%02d/%02d/%04d %02d:%02d:%02d\",\n\t\"Env_temp\":%.2f,\n\t\"Env_Humi\":%.2f,\n\t\"Env_Lux\":%d\n\t\"Soil_Humi\": %.1f\n}",
                data_temp.time_data.time_day,
                data_temp.time_data.time_month,
                data_temp.time_data.time_year,
                data_temp.time_data.time_hour,
                data_temp.time_data.time_min,
                data_temp.time_data.time_sec,

                data_temp.Env_temp,
                data_temp.Env_Humi,
                data_temp.Env_Lux,
                data_temp.Soil_humi);
        ESP_LOGI(__func__, "%s",data_string);
        ESP_LOGI(__func__, "SHT: %d, BH1750: %d, ADC: %d, RTC: %d", Device_status.sht_status, Device_status.lux_status, Device_status.soil_status, Device_status.rtc_status);

        xQueueSendToBack(dataHTTP_queue, &data_temp, 500/portMAX_DELAY);
        xQueueSendToBack(dataMQTT_queue, &data_temp, 500/portMAX_DELAY);
        ESP_LOGI(__func__, "HTTP waiting to read %d, Available space %d", uxQueueMessagesWaiting(dataHTTP_queue), uxQueueSpacesAvailable(dataHTTP_queue));
        ESP_LOGI(__func__, "MQTT waiting to read %d, Available space %d", uxQueueMessagesWaiting(dataMQTT_queue), uxQueueSpacesAvailable(dataMQTT_queue));
        xTaskCreatePinnedToCore(send_data_http, "send_data_http", 2048*2, NULL, 8, &send_http_handle, tskNO_AFFINITY);
        xTaskCreatePinnedToCore(send_data_mqtt, "send_data_mqtt", 2048*2, NULL, 8, &send_mqtt_handle, tskNO_AFFINITY);
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(GET_DATA_PERIOD * 60000));
    }
}

void send_data_http(void *pvParameters){
    Data_t data_http = {};
    TickType_t last_wakeup = xTaskGetTickCount();
    while(1){
        if(xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY) && WIFI_CONNECTED_BIT)
        {
            vTaskDelay(5000/portTICK_RATE_MS);
            int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);
			if(err != 0 || res == NULL) {
				ESP_LOGE(__func__, "DNS lookup failed err=%d res=%p", err, res);
				vTaskDelay((TickType_t)(1000 / portTICK_RATE_MS));
				continue;
			}
            addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
			ESP_LOGI(__func__, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

			s = socket(res->ai_family, res->ai_socktype, 0);
			if(s < 0) {
				ESP_LOGE(__func__, "... Failed to allocate socket.");
                xEventGroupSetBits(http_event_group, HTTP_DISCONNECTED_BIT);
				freeaddrinfo(res);
				// vTaskDelay((TickType_t)(1000 / portTICK_RATE_MS));
				continue;
			}
			ESP_LOGI(__func__, "... allocated socket");

			if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
				ESP_LOGE(__func__, "... socket connect failed errno=%d", errno);
                xEventGroupSetBits(http_event_group, HTTP_DISCONNECTED_BIT);
				close(s);
				freeaddrinfo(res);
				// vTaskDelay((TickType_t)(1000 / portTICK_RATE_MS));
				continue;
			}

			ESP_LOGI(__func__, "... connected");
			xEventGroupSetBits(http_event_group, HTTP_CONNECTED_BIT);
			freeaddrinfo(res);

            ESP_LOGI(__func__,"Send data HTTP");
			if(xEventGroupWaitBits(http_event_group, HTTP_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY) && HTTP_CONNECTED_BIT)
        	{
                if(uxQueueMessagesWaiting(dataHTTP_queue) != 0){
                    if (xQueueReceive(dataHTTP_queue, &data_http, portMAX_DELAY) == pdPASS) {
                        if (xSemaphoreTake(send_data_mutex, portMAX_DELAY) == pdTRUE){   
                            memset(REQUEST, 0, 512);

                            sprintf(REQUEST, "GET https://api.thingspeak.com/update?api_key=%s&field1=%.2f&field2=%.2f&field3=%d\n\n\n", API_KEY, data_http.Env_temp, data_http.Env_Humi, data_http.Env_Lux);

                            ESP_LOGI(__func__, "%s",REQUEST);

                            ESP_LOGI(__func__, "HTTP waiting to read %d, Available space %d \n", uxQueueMessagesWaiting(dataHTTP_queue), uxQueueSpacesAvailable(dataHTTP_queue));
                            
							if ( write(s, REQUEST, strlen(REQUEST)) < 0) {
								ESP_LOGE(__func__, "HTTP client publish message failed");
							} else {
								ESP_LOGI(__func__, "HTTP client publish message success\n");
							}
                            xSemaphoreGive(send_data_mutex);
                        }
                    }
                }
            }
            close(s);
        }
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(SEND_DATA_PERIOD * 60000));
        vTaskDelete(NULL);
    }
}

void send_data_mqtt(void *pvParameters){
    Data_t data_mqtt = {};
    TickType_t last_wakeup = xTaskGetTickCount();
    while(1){
        if(xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY) && MQTT_CONNECTED_BIT){
            if(uxQueueMessagesWaiting(dataMQTT_queue) != 0){
                if (xQueueReceive(dataMQTT_queue, &data_mqtt, portMAX_DELAY) == pdPASS) {
                    if (xSemaphoreTake(send_data_mutex, portMAX_DELAY) == pdTRUE){
                        
                        esp_err_t error = 0;
						int msg_id;
						WORD_ALIGNED_ATTR char mqttMessage[512];
                        sprintf(mqttMessage, "{\n\t\"Time_real_Date\":\"%02d/%02d/%02d %02d:%02d:%02d\",\n\t\"Soil_temp\":%.2f,\n\t\"Soil_humi\":%.2f,\n\t\"Soil_pH\":%.2f,\n\t\"Soil_Nito\":%.2f,\n\t\"Soil_Kali\":%.2f,\n\t\"Soil_Phosp\":%.2f,\n\t\"Env_temp\":%.2f,\n\t\"Env_Humi\":%.2f,\n\t\"Env_Lux\":%d\n}",
								data_mqtt.time_data.time_day,
                                data_mqtt.time_data.time_month,
                                data_mqtt.time_data.time_year,
                                data_mqtt.time_data.time_hour,
                                data_mqtt.time_data.time_min,
                                data_mqtt.time_data.time_sec,

								data_mqtt.Soil_temp,
								0.00,
								0.00, 0.00, 0.00, 0.00,
								data_mqtt.Env_temp,
                                data_mqtt.Env_Humi,
                                data_mqtt.Env_Lux);
						ESP_LOGI(__func__, "%s",mqttMessage);
						error = esp_mqtt_client_publish(mqttClient_handle, (const char *)CONFIG_MQTT_TOPIC, mqttMessage, 0, 0, 0);

                        xSemaphoreGive(send_data_mutex);
                    }
                }
            }
        }
        vTaskDelete(NULL);
    }
}

void I2C_init(void){
    ESP_ERROR_CHECK(i2cdev_init());
    memset(&bh1750, 0, sizeof(i2c_dev_t));
    memset(&ds3231, 0, sizeof(i2c_dev_t));
    memset(&sht41, 0, sizeof(sht4x_t));
}

static void check_efuse(void)
{
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}
static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void ADC_Init(void){
    check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);
    Device_status.soil_status = true;
}
/*----------------------------------------------------Wifi-------------------------------------------------*/
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	printf("id %d\n", event_id);
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(__func__,"WiFi connecting ... \n");
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(__func__,"WiFi Connected ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(__func__,"Try to WiFi connection ... \n");
        esp_wifi_connect();
        break;
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(__func__,"WiFi got IP ... \n\n");
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
		ESP_LOGI(__func__,"Wi-Fi connected HTTP start\n");
        mqtt_app_start();
        ESP_LOGI(__func__,"Wi-Fi connected MQTT start\n");
        break;
    default:
        break;
    }
}

void wifi_connection(void){

    esp_err_t error = nvs_flash_init();
	if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
		error = nvs_flash_init();
	}
	ESP_ERROR_CHECK_WITHOUT_ABORT(error);

    wifi_event_group = xEventGroupCreate();
	http_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta(); 
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); 
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = SSID,
            .password = PASS}};
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_start();
    esp_wifi_connect();
}

//mqtt
static void mqtt_event_handler (void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    ESP_LOGD(__func__, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    switch((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
        xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        ESP_LOGI(__func__, "MQTT_EVENT_CONNECTED");
        printf("Topic: %s\n", CONFIG_MQTT_TOPIC);
        break;

        case MQTT_EVENT_DISCONNECTED:
        xEventGroupSetBits(mqtt_event_group, MQTT_DISCONNECTED_BIT);
        ESP_LOGE(__func__, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(__func__, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGE(__func__, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGE(__func__, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGE(__func__, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGE(__func__, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(__func__, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;

    default:
        ESP_LOGI(__func__, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void) {
	printf("mqtt\n");
	mqtt_event_group = xEventGroupCreate();
    const esp_mqtt_client_config_t mqtt_Config = {
        .host = CONFIG_BROKER_HOST,
        .uri = CONFIG_BROKER_URI,
        .port = CONFIG_BROKER_PORT,
        .username = CONFIG_MQTT_USERNAME,
        .password = CONFIG_MQTT_PASSWORD,
    };
    ESP_LOGI(__func__, "Free memory: %d bytes", esp_get_free_heap_size());
    mqttClient_handle = esp_mqtt_client_init(&mqtt_Config);
     /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqttClient_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, mqttClient_handle);
    esp_mqtt_client_start(mqttClient_handle);
    esp_read_mac(MAC_address, ESP_MAC_WIFI_STA); // Get MAC address of ESP32
//    xTaskCreatePinnedToCore(send_data_mqtt, "send_data_http", 2048 * 2, NULL, 4, &send_data_mqtt_task, tskNO_AFFINITY);
}