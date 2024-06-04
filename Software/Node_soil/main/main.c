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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "lora.h"
#include "general.h"
#include <ds3231.h>
#include <ads111x.h>
#include <ds18x20.h>


TaskHandle_t send_lora_handle = NULL;
TaskHandle_t rtc_handle = NULL;
TaskHandle_t Soil_temp_handle = NULL;
TaskHandle_t Soil_humi_handle = NULL;

SemaphoreHandle_t I2C_mutex = NULL;

static i2c_dev_t devices[CONFIG_EXAMPLE_DEV_COUNT];
static float gain_val;

static const gpio_num_t SENSOR_GPIO = CONFIG_EXAMPLE_ONEWIRE_GPIO;
static const onewire_addr_t SENSOR_ADDR = CONFIG_EXAMPLE_DS18X20_ADDR;

volatile Data_t Data;

void send_lora_task(void *pvParameters)
{
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	Data_t lora_data = Data;
	uint8_t buf[256]; // Maximum Payload size of SX1276/77/78/79 is 255
	while(1) {
		int send_len = sprintf((char *)buf,"%02d-%02d-%04d %02d:%02d:%02d %.02f-%.04f",
			lora_data.time_data.time_day, 
			lora_data.time_data.time_month,
			lora_data.time_data.time_year, 
			lora_data.time_data.time_hour, 
			lora_data.time_data.time_min, 
			lora_data.time_data.time_sec,
			lora_data.Soil_temp,
			lora_data.Soil_humi
		);
		lora_send_packet(buf, send_len);
		ESP_LOGI(pcTaskGetName(NULL), "%d byte packet sent: %s", send_len, buf);
		int lost = lora_packet_lost();
		if (lost != 0) {
			ESP_LOGW(pcTaskGetName(NULL), "%d packets lost", lost);
		}
		esp_deep_sleep_start();
		vTaskDelete(NULL);
	}
}

void ads111x_task(void *pvParameters)
{
    memset(devices, 0, sizeof(devices));
	gain_val = ads111x_gain_values[GAIN];

    for (size_t i = 0; i < CONFIG_EXAMPLE_DEV_COUNT; i++)
    {
        ESP_ERROR_CHECK(ads111x_init_desc(&devices[i], ADS111X_ADDR_GND, 0, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));

        ESP_ERROR_CHECK(ads111x_set_mode(&devices[i], ADS111X_MODE_CONTINUOUS));    // Continuous conversion mode
        ESP_ERROR_CHECK(ads111x_set_data_rate(&devices[i], ADS111X_DATA_RATE_32)); // 32 samples per second
        ESP_ERROR_CHECK(ads111x_set_input_mux(&devices[i], ADS111X_MUX_0_GND));    // positive = AIN0, negative = GND
        ESP_ERROR_CHECK(ads111x_set_gain(&devices[i], GAIN));
    }

    while (1)
    {
		if(xSemaphoreTake(I2C_mutex, portMAX_DELAY) == pdTRUE){
			for (size_t i = 0; i < CONFIG_EXAMPLE_DEV_COUNT; i++)
			{
				int16_t raw = 0;
				if (ads111x_get_value(&devices[i], &raw) == ESP_OK){
					float voltage = gain_val / ADS111X_MAX_VALUE * raw;
					Data.Soil_humi = voltage;
					printf("[%u] Raw ADC value: %d, voltage: %.04f volts\n", i, raw, voltage);
				} else {
					printf("[%u] Cannot read ADC value\n", i);
				}
			}
			xSemaphoreGive(I2C_mutex);
		}
		xTaskNotifyGive(send_lora_handle);
		vTaskDelete(NULL);
    }
}

void ds18b20_task(void *pvParameter)
{
    gpio_set_pull_mode(SENSOR_GPIO, GPIO_PULLUP_ONLY);

    float temperature;
    while (1)
    {
        if (ds18x20_measure_and_read(SENSOR_GPIO, SENSOR_ADDR, &temperature) != ESP_OK)
            ESP_LOGE(__func__, "Could not read from sensor");
        else
            ESP_LOGI(__func__, "Sensor %08" PRIx32 "%08" PRIx32 ": %.2f°C",
                    (uint32_t)(SENSOR_ADDR >> 32), (uint32_t)SENSOR_ADDR, temperature);

		Data.Soil_temp = temperature;

		xTaskNotifyGive(send_lora_handle);
		vTaskDelete(NULL);
    }
}

void rtc_task(void *pvParameters)
{
	i2c_dev_t dev;
    memset(&dev, 0, sizeof(i2c_dev_t));

    ESP_ERROR_CHECK(ds3231_init_desc(&dev, 0, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));
	struct tm time = {};
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

		xTaskCreatePinnedToCore(ads111x_task, "ads111x_task", 2048*2, NULL, 5, &Soil_humi_handle, tskNO_AFFINITY);
		xTaskCreatePinnedToCore(ds18b20_task, "ds18b20_task", 2048*2, NULL, 5, &Soil_temp_handle, tskNO_AFFINITY);
		xTaskCreatePinnedToCore(send_lora_task, "send_lora_task", 2048*2, NULL, 6, &send_lora_handle, tskNO_AFFINITY);

		// vTaskDelay(pdMS_TO_TICKS(5000));
		vTaskDelete(NULL);
	}
}

void app_main(void)
{
	esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    if (lora_init() == 0) {
		ESP_LOGE(pcTaskGetName(NULL), "Does not recognize the module");
	} else {
		lora_config();
	}
    
	ESP_ERROR_CHECK(i2cdev_init());
	I2C_mutex = xSemaphoreCreateMutex();

	xTaskCreatePinnedToCore(rtc_task, "rtc_task", 2048*2, NULL, 4, &rtc_handle, tskNO_AFFINITY);
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