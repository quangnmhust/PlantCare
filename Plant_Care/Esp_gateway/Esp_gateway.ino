#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

const char* ssid = "DN Bao Tam 2";
const char* password = "28061997";
// const char* ssid = "minhquangng";
// const char* password = "TBH123456";
const char* mqttServer = "sanslab.ddns.net";
const int mqttPort = 1883;
const char* mqttUser = "admin"; 
const char* mqttPassword = "123";
const char* mqttTopic = "modelParam";

WiFiClient mqttclient;
PubSubClient client(mqttclient);

RF24 radio(4, 5, 18, 19, 23); //CE-CS-SCK-miso-mosi
const uint64_t addr = 0xE8E8F0F0E1LL;

typedef struct Data_manager1{
  uint16_t Time_year;
	uint8_t Time_month;
	uint8_t Time_day;
	uint8_t Time_hour;
	uint8_t Time_min;
	uint8_t Time_sec;
  float Env_temp;
  float Env_Humi;
  float Env_Lux;
} Data_1_t;

typedef struct Data_manager2{
  float Soil_temp;
  float Soil_humi;
  float Soil_pH;
  float Soil_Nito;
  float Soil_Phosp;
  float Soil_Kali;
} Data_2_t;

Data_1_t recvData1={};
Data_2_t recvData2={};

QueueHandle_t data_queue1 = NULL;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Topic: "+String(topic));
  Serial.println("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}

void setup_wifi(void) {
  // //Init WiFi as Station, start SmartConfig
  // WiFi.mode(WIFI_AP_STA);
  // WiFi.beginSmartConfig();

  // //Wait for SmartConfig packet from mobile
  // Serial.println("Waiting for SmartConfig.");
  // while (!WiFi.smartConfigDone()) {
  //   delay(500);
  //   Serial.print(".");
  // }

  // Serial.println("");
  // Serial.println("SmartConfig received.");

  // //Wait for WiFi to connect to AP
  // Serial.println("Waiting for WiFi");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }

  // Serial.println("WiFi Connected.");

  // Serial.print("IP Address: ");
  // Serial.println(WiFi.localIP());
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void esp_connect_mqtt(void){
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("Connected!");
      // client.publish("modelParam","hello world");
    } else {
      Serial.println("failed, rc=" + String(client.state()));
      Serial.println("Try again in 2 seconds");
      delay(2000);
    }
  }
}

void setup(void){
  Serial.begin(9600);

  // setup_wifi();
  // esp_connect_mqtt();

  data_queue1 = xQueueCreate(20, sizeof(Data_1_t));
  if (data_queue1 != NULL)
  {
    Serial.println("Data_queue1 created");
  }

  Serial.println(sizeof(Data_1_t)); //20
  Serial.println(sizeof(Data_2_t)); //24

  pinMode(2, OUTPUT);
  if(!radio.begin()){
    Serial.println("RF Failed");
  } else {
    Serial.println("RF");
  }
    radio.setChannel(2);
    //  radio.setPayloadSize(24);
    radio.setDataRate(RF24_250KBPS);
    radio.openReadingPipe(1,addr);
    radio.startListening();
  }

void loop(void){
//     digitalWrite(2, LOW);
//     if (radio.available()){
//     Data_t recvData={};
//     radio.read(&recvData, sizeof(Data_t));
//     Serial.println(recvData.Env_temp);
//     if (xQueueSendToBack(data_queue, (void *)&recvData, 1000/portMAX_DELAY) == pdTRUE  ){
// 			Serial.print("MQTT data waiting to read ");
//       Serial.print(uxQueueMessagesWaiting(data_queue));
//       Serial.print(", Available space ");
//       Serial.println(uxQueueSpacesAvailable(data_queue));
//       // xTaskCreatePinnedToCore(MQTT_task, "MQTT_task", 1024 * 4, NULL, 7, &MQTT_task_handle, tskNO_AFFINITY);
//       xTaskCreatePinnedToCore(RF24_task, "RF24_task", 1024 * 4, NULL, 7, &RF24_task_handle, tskNO_AFFINITY);
// 		}
//     digitalWrite(2, HIGH);
//     WORD_ALIGNED_ATTR char mqttMessage[512];
//     sprintf(mqttMessage, "{\n\t\"Time_real_Date\":\"%02d/%02d/%02d %02d:%02d:%02d\",\n\t\"Soil_temp\":%.2f,\n\t\"Soil_humi\":%.2f,\n\t\"Soil_pH\":%.2f,\n\t\"Soil_Nito\":%.2f,\n\t\"Soil_Kali\":%.2f,\n\t\"Soil_Phosp\":%.2f,\n\t\"Env_temp\":%.2f,\n\t\"Env_Humi\":%.2f,\n\t\"Env_Lux\":%.2f\n}",
//             recvData.Time_day,
//             recvData.Time_month,
//             recvData.Time_year,
//             recvData.Time_hour,
//             recvData.Time_min,
//             recvData.Time_sec,
//             recvData.Soil_temp,
//             recvData.Soil_humi,
//             recvData.Soil_pH,
//             recvData.Soil_Nito,
//             recvData.Soil_Phosp,
//             recvData.Soil_Kali,
//             recvData.Env_temp,
//             recvData.Env_Humi,
//             recvData.Env_Lux
//           );
//     Serial.println(mqttMessage);
//     // Serial.println(client.publish("modelParam",mqttMessage));
//     delay(10);
//  }
//  else{
//   //Serial.println("No radio available");
//  }
}