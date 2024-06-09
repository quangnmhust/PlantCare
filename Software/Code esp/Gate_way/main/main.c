/* The example of ESP-IDF
 *
 * This sample code is in the public domain.
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/event_groups.h"

#include "esp_log.h"

#include "lora.h"
#include "general.h"

static const char *TAG = "GATE_WAY";

TaskHandle_t recv_lora_handle = NULL;
TaskHandle_t recv_data_handle = NULL;
TaskHandle_t send_sim_handle = NULL;

QueueHandle_t datarecv_queue;
QueueHandle_t datasim_queue;

void recv_lora_task(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(NULL), "Start");
	uint8_t buf[256]; // Maximum Payload size of SX1276/77/78/79 is 255
	while(1) {
		lora_receive(); // put into receive mode
		if (lora_received()) {
			int rxLen = lora_receive_packet(buf, sizeof(buf));
			if(rxLen == 96){
				ESP_LOGI(pcTaskGetName(NULL), "%d byte:[%.*s]", rxLen, rxLen, buf);
				xQueueSendToBack(datarecv_queue, &buf, 100/portMAX_DELAY);
				ESP_LOGI(TAG, "Uart to read %d, Available space %d", uxQueueMessagesWaiting(datarecv_queue), uxQueueSpacesAvailable(datarecv_queue));
			} else {
				continue;
			}
		}
		vTaskDelay(1); // Avoid WatchDog alerts
	} // end while
}

void recv_data_task(void *pvParameters)
{
	uint8_t buf_de[256];
	Data_t data_recv;
	TickType_t last_wakeup = xTaskGetTickCount();
	while(1){
		if(uxQueueMessagesWaiting(datarecv_queue) != 0){
			if (xQueueReceive(datarecv_queue, &buf_de, portMAX_DELAY) == pdPASS)
			{
				printf("%d byte: s%.*s\n", 96, 96, buf_de);
				sscanf((char *)buf_de, "%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x",
					&data_recv.id,
					(unsigned int *)&data_recv.time_data.time_day,
					(unsigned int *)&data_recv.time_data.time_month,
					(unsigned int *)&data_recv.time_data.time_year,
					(unsigned int *)&data_recv.time_data.time_hour,
					(unsigned int *)&data_recv.time_data.time_min,
					(unsigned int *)&data_recv.time_data.time_sec,
					(unsigned int *)&data_recv.Soil_temp,
					(unsigned int *)&data_recv.Soil_humi,
					(unsigned int *)&data_recv.Env_temp,
					(unsigned int *)&data_recv.Env_humi,
					(unsigned int *)&data_recv.Env_lux
				);

				xQueueSendToBack(datasim_queue, &data_recv, 100/portMAX_DELAY);
				ESP_LOGI(TAG, "Sim to read %d, Available space %d", uxQueueMessagesWaiting(datasim_queue), uxQueueSpacesAvailable(datasim_queue));

				xTaskCreatePinnedToCore(send_sim_task, "send_sim_task", 2048*2, NULL, 5, &send_sim_handle, tskNO_AFFINITY);
			}
		}
		vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(1000));
	}
}

void send_sim_task(void *pvParameters)
{	
	Data_t data_sim;
	while(1){
		if(uxQueueMessagesWaiting(datasim_queue) != 0){
			if (xQueueReceive(datasim_queue, &data_sim, portMAX_DELAY) == pdPASS)
			{
				printf("Node: %d\n{\n  Data: %02d/%02d/%04d %02d:%02d:%02d %.2f-%.2f-%.2f-%.2f-%02d\n}\n", 
					data_sim.id, 
					data_sim.time_data.time_day,
					data_sim.time_data.time_month,
					data_sim.time_data.time_year,
					data_sim.time_data.time_hour,
					data_sim.time_data.time_min,
					data_sim.time_data.time_sec,
					data_sim.Soil_temp,
					data_sim.Soil_humi,
					data_sim.Env_temp,
					data_sim.Env_humi,
					data_sim.Env_lux
				);
			}
		}
		vTaskDelete(NULL);
	}
}

void app_main()
{
	if (lora_init() == 0) {
		ESP_LOGE(pcTaskGetName(NULL), "Does not recognize the module");
		while(1) {
			vTaskDelay(1);
		}
	} else {
		datarecv_queue = xQueueCreate(20, 256);
		datasim_queue = xQueueCreate(20, sizeof(Data_t));
		ESP_LOGI(TAG, "Create Queue success.");
		lora_config();
	}

	xTaskCreatePinnedToCore(recv_lora_task, "recv_lora_task", 2048*2, NULL, 4, &recv_lora_handle, tskNO_AFFINITY);
	xTaskCreatePinnedToCore(recv_data_task, "recv_data_task", 2048*2, NULL, 4, &recv_data_handle, tskNO_AFFINITY);
	xTaskCreatePinnedToCore(send_sim_task, "send_sim_task", 2048*2, NULL, 5, &send_sim_handle, tskNO_AFFINITY);
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