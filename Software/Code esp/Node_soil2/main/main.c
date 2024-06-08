#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "sdkconfig.h"

#include <esp_log.h>
#include <esp_err.h>
#include "esp32/ulp.h"
#include "esp_sleep.h"
#include <esp_system.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "lora.h"
#include "general.h"
#include <ds3231.h>
#include <bh1750.h>
#include <sht4x.h>

static const char *TAG = "Env_node";

TaskHandle_t send_lora_handle = NULL;
TaskHandle_t rtc_handle = NULL;
TaskHandle_t Env_sensor_handle = NULL;
TaskHandle_t Env_lux_handle = NULL;

SemaphoreHandle_t I2C_mutex = NULL;

volatile Data_t Data;

void send_lora_task(void *pvParameters)
{
	gpio_set_level(BLINK_GPIO, 1);
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	Data_t lora_data = Data;
	uint8_t buf[256]; // Maximum Payload size of SX1276/77/78/79 is 255
	while(1) {
		int send_len = sprintf((char *)buf,"%02d-%02d-%04d %02d:%02d:%02d %.02f-%.02f-%.02d",
			lora_data.time_data.time_day, 
			lora_data.time_data.time_month,
			lora_data.time_data.time_year, 
			lora_data.time_data.time_hour, 
			lora_data.time_data.time_min, 
			lora_data.time_data.time_sec,
			lora_data.Env_temp,
			lora_data.Env_humi,
			lora_data.Env_lux
		);
		lora_send_packet(buf, send_len);
		ESP_LOGI(pcTaskGetName(NULL), "%d byte packet sent: %s", send_len, buf);
		int lost = lora_packet_lost();
		if (lost != 0) {
			ESP_LOGW(pcTaskGetName(NULL), "%d packets lost", lost);
		}
		esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
		esp_deep_sleep_start();
		vTaskDelete(NULL);
	}
}

void bh1750_task(void *pvParameters)
{
	uint16_t lux;

	i2c_dev_t bh1750;
	memset(&bh1750, 0, sizeof(i2c_dev_t)); // Zero descriptor
    ESP_ERROR_CHECK(bh1750_init_desc(&bh1750, BH1750_ADDR_LO, 0, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));
    ESP_ERROR_CHECK(bh1750_setup(&bh1750, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH));

    while (1)
    {
		if(xSemaphoreTake(I2C_mutex, portMAX_DELAY) == pdTRUE){
			if (bh1750_read(&bh1750, &lux) != ESP_OK){
				printf("Could not read lux data\n");
			} else {
				Data.Env_lux = lux;
				printf("Lux: %d\n", Data.Env_lux);
			}
			xSemaphoreGive(I2C_mutex);
		}
		xTaskNotifyGive(send_lora_handle);
		vTaskDelete(NULL);
    }
}

void sht40_task(void *pvParameter)
{
	float temperature;
    float humidity;

	sht4x_t sht40;
	memset(&sht40, 0, sizeof(sht4x_t));
    ESP_ERROR_CHECK(sht4x_init_desc(&sht40, 0, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));
    ESP_ERROR_CHECK(sht4x_init(&sht40));
	
    while (1)
    {
		if(xSemaphoreTake(I2C_mutex, portMAX_DELAY) == pdTRUE){
			ESP_ERROR_CHECK(sht4x_measure(&sht40, &temperature, &humidity));
			Data.Env_temp = temperature;
			Data.Env_humi = humidity;
        	printf("sht4x Sensor: %.2f °C, %.2f %%\n", Data.Env_temp, Data.Env_humi);
			xSemaphoreGive(I2C_mutex);
		}
		xTaskNotifyGive(send_lora_handle);
		vTaskDelete(NULL);
    }
}

void rtc_task(void *pvParameters)
{
	struct tm time = {};

	i2c_dev_t dev;
	memset(&dev, 0, sizeof(i2c_dev_t));
    ESP_ERROR_CHECK(ds3231_init_desc(&dev, 0, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));

	while(1){
		if(xSemaphoreTake(I2C_mutex, portMAX_DELAY) == pdTRUE){
			if (ds3231_get_time(&dev, &time) != ESP_OK)
			{
				printf("Could not get time\n");
				continue;
			}

			Data.time_data.time_year = time.tm_year + 1900;
			Data.time_data.time_month = time.tm_mon + 1;
			Data.time_data.time_day = time.tm_mday;
			Data.time_data.time_hour = time.tm_hour;
			Data.time_data.time_min = time.tm_min;
			Data.time_data.time_sec = time.tm_sec;

			printf("%02d-%02d-%04d %02d:%02d:%02d\n", 
				Data.time_data.time_day, 
				Data.time_data.time_month,
				Data.time_data.time_year, 
				Data.time_data.time_hour, 
				Data.time_data.time_min, 
				Data.time_data.time_sec
			);  
			xSemaphoreGive(I2C_mutex);
		}

		xTaskCreatePinnedToCore(bh1750_task, "bh1750_task", 2048*2, NULL, 5, &Env_lux_handle, tskNO_AFFINITY);
		xTaskCreatePinnedToCore(sht40_task, "sht40_task", 2048*2, NULL, 5, &Env_sensor_handle, tskNO_AFFINITY);
		xTaskCreatePinnedToCore(send_lora_task, "send_lora_task", 2048*2, NULL, 6, &send_lora_handle, tskNO_AFFINITY);

		// vTaskDelay(pdMS_TO_TICKS(5000));
		vTaskDelete(NULL);
	}
}

void app_main(void)
{
    if (lora_init() == 0) {
		ESP_LOGE(pcTaskGetName(NULL), "Does not recognize the module");
	} else {
		lora_config();
	}

	configure_led();

	ESP_ERROR_CHECK(i2cdev_init());
	I2C_mutex = xSemaphoreCreateMutex();

	xTaskCreatePinnedToCore(rtc_task, "rtc_task", 2048*2, NULL, 4, &rtc_handle, tskNO_AFFINITY);
	// xTaskCreatePinnedToCore(send_lora_task, "send_lora_task", 2048*2, NULL, 6, &send_lora_handle, tskNO_AFFINITY);
}

void lora_config(){
    #if CONFIG_169MHZ
        ESP_LOGI(pcTaskGetName(NULL), "Frequency is 169MHz");
        lora_set_frequency(169e6); // 169MHz
    #elif CONFIG_433MHZ
        ESP_LOGI(pcTaskGetName(NULL), "Frequency is 433MHz");
        lora_set_frequency(433e6); // 433MHz
    #elif CONFIG_470MHZ
        ESP_LOGI(pcTaskGetName(NULL), "Frequency is 470MHz");
        lora_set_frequency(470e6); // 470MHz
    #elif CONFIG_866MHZ
        ESP_LOGI(pcTaskGetName(NULL), "Frequency is 866MHz");
        lora_set_frequency(866e6); // 866MHz
    #elif CONFIG_915MHZ
        ESP_LOGI(pcTaskGetName(NULL), "Frequency is 915MHz");
        lora_set_frequency(915e6); // 915MHz
    #elif CONFIG_OTHER
        ESP_LOGI(pcTaskGetName(NULL), "Frequency is %dMHz", CONFIG_OTHER_FREQUENCY);
        long frequency = CONFIG_OTHER_FREQUENCY * 1000000;
        lora_set_frequency(frequency);
    #endif

	lora_enable_crc();

	int cr = 1;
	int bw = 7;
	int sf = 7;
    #if CONFIF_ADVANCED
        cr = CONFIG_CODING_RATE
        bw = CONFIG_BANDWIDTH;
        sf = CONFIG_SF_RATE;
    #endif

	lora_set_coding_rate(cr);
	//lora_set_coding_rate(CONFIG_CODING_RATE);
	//cr = lora_get_coding_rate();
	ESP_LOGI(pcTaskGetName(NULL), "coding_rate=%d", cr);

	lora_set_bandwidth(bw);
	//lora_set_bandwidth(CONFIG_BANDWIDTH);
	//int bw = lora_get_bandwidth();
	ESP_LOGI(pcTaskGetName(NULL), "bandwidth=%d", bw);

	lora_set_spreading_factor(sf);
	//lora_set_spreading_factor(CONFIG_SF_RATE);
	//int sf = lora_get_spreading_factor();
	ESP_LOGI(pcTaskGetName(NULL), "spreading_factor=%d", sf);
}

void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}