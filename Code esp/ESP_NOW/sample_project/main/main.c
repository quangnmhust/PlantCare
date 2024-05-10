#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "esp_crc.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "espnow_example.h"

static uint8_t peer_address[ESP_NOW_ETH_ALEN] = { 0x94, 0xE6, 0x86, 0x08, 0x7F, 0xCC };

static const char * TAG = "ESP_NOW_INIT";

/* WiFi should start before using ESPNOW */
static void wifi_init_sta(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

}

static void example_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{

    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    if(status == ESP_NOW_SEND_SUCCESS){
        ESP_LOGI(TAG, "SUCCESS");
    } 
    else
    {
        ESP_LOGE(TAG, "FAIL");
    }
    


}
static void example_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{   
    example_espnow_event_t evt;
    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    ESP_LOGI(TAG, "Receive data: %s", data);
}

static esp_err_t register_peer(uint8_t *peer_addr){

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESP_IF_WIFI_STA;
    peer->encrypt = false;
    memcpy(peer->peer_addr, peer_addr, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);
    return ESP_OK;
} 

static esp_err_t espnow_send_data(const uint8_t *peer_addr, const uint8_t *data, size_t len){
    esp_now_send(peer_addr, data, len);
    return ESP_OK;
}

static esp_err_t init_esp_now(void){

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(example_espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(example_espnow_recv_cb) );
    ESP_ERROR_CHECK( register_peer(peer_address));

    ESP_LOGI(TAG, "ESP_NOW init completed!");
    return ESP_OK;
}

void app_main(void)
{
    wifi_init_sta();
    ESP_ERROR_CHECK(init_esp_now());

    char dataR[12] = "255|0|0";

    uint8_t count = 0;
    while (1)
    {   
        count++;
        if(count >2){
            count = 0;
        }
        switch (count)
        {   
        case 0:
            espnow_send_data(peer_address, &dataR, 12);
            break;
        case 1:
            espnow_send_data(peer_address, &dataR, 12);
            break;
        case 2:
            espnow_send_data(peer_address, &dataR, 12);
            break;
        default:
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
}