#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Arduino.h>
#include "String.h"
#include "EEPROM.h"
#include <ThingSpeak.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define LED_GPIO 2
#define LENGTH(x) (strlen(x) + 1)
#define EEPROM_SIZE 200

// String ssid = "minhquangng";
// String pss = "TBH123456";
String ssid = "Tom Bi";
String pss = "TBH123456";
const int myChannelNumber = 2515584;
const char * myWriteAPIKey = "8TGOAVM92D494KS1";
const char* mqttServer = "sanslab.ddns.net";
const int mqttPort = 1883;
const char* mqttUser = "admin";
const char* mqttPassword = "123";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 6*3600;
const int   daylightOffset_sec = 3600;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// WiFiClient  httpclient;
WiFiClient mqttclient;
PubSubClient client(mqttclient);
RF24 radio(4, 5, 18, 19, 23); //CE-CS-SCK-miso-mosi

typedef struct Data_manager_full{
  uint16_t Time_year;
	uint8_t Time_month;
	uint8_t Time_day;
	uint8_t Time_hour;
	uint8_t Time_min;
	uint8_t Time_sec;

  float Soil_temp;
  float Soil_humi;
  float Soil_pH;
  float Soil_Nito;
  float Soil_Phosp;
  float Soil_Kali;
  float Env_temp;
  float Env_Humi;
  float Env_Lux;
} Data_f_t;

volatile Data_f_t data_full;
volatile bool mqtt_check;
volatile bool http_check;

typedef struct Data_manager{
  int index;
  uint16_t Time_year;
	uint8_t Time_month;
	uint8_t Time_day;
	uint8_t Time_hour;
	uint8_t Time_min;
	uint8_t Time_sec;

  float Soil_temp;
  float Soil_humi;
  float Soil_pH;
  float Soil_Nito;
  float Soil_Phosp;
  float Soil_Kali;
} Data_t;

Data_t SensorData;

const uint64_t address = 0xF0F0F0F0E1LL;

SemaphoreHandle_t MQTT_semaphore = NULL;

TaskHandle_t Display_task_handle;
TaskHandle_t MQTT_task_handle;
TaskHandle_t RevData_task_handle;

QueueHandle_t data_queue = NULL;

char time_buff[64];
char mqttMessage_check[512];

void MQTT_task(void *pvParameters);
void Display_task(void *pvParameters);
void RevData_task(void *pvParameters);

void writeStringToFlash(const char* toStore, int startAddr) {
  int i = 0;
  for (; i < LENGTH(toStore); i++) {
    EEPROM.write(startAddr + i, toStore[i]);
  }
  EEPROM.write(startAddr + i, '\0');
  EEPROM.commit();
}

String readStringFromFlash(int startAddr) {
  char in[128]; // char array of size 128 for reading the stored data 
  int i = 0;
  for (; i < 128; i++) {
    in[i] = EEPROM.read(startAddr + i);
  }
  return String(in);
}

void callback(char* topic, byte* payload, unsigned int length) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Topic: "+String(topic));
  display.setCursor(0,10);
  display.println("Message:");
  display.display();
  for (int i = 0; i < length; i++) {
    int a,b;
    a= (i%21)*6;
    b= (i/21)*10+20;
    display.setCursor(a,b);
    Serial.println((char)payload[i]);
    mqttMessage_check[i]=(char)payload[i];
    display.display();
  }
}

void esp_connect_wifi(void){
  // WiFi.begin(ssid, password);
  WiFi.begin(ssid.c_str(), pss.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    // if(ssid == ""){
    //   WiFi.mode(WIFI_AP_STA);
    //   WiFi.beginSmartConfig();
    //   Serial.println("Waiting for SmartConfig.");
    //   while (!WiFi.smartConfigDone()) {
    //     delay(500);
    //     Serial.print(".");
    //   }
    //   Serial.println("");
    //   Serial.println("SmartConfig received.");
    // }
    digitalWrite (LED_GPIO, HIGH);
    display.setCursor(0,0);
    display.fillRect(0, 0, SCREEN_WIDTH, 9, BLACK);
    display.println("Connecting to WiFi");
    display.display();
    display.setCursor(0,20);
    display.println("SSID:"+String(ssid));
    display.setCursor(0,30);
    display.println("PASS:"+String(pss));
    display.display();
    delay(125);
    digitalWrite (LED_GPIO, LOW);
    display.setCursor(0,0);
    display.fillRect(0, 0, SCREEN_WIDTH, 9, BLACK);
    display.println("Connecting to WiFi.");
    display.display();
    delay(125);
    digitalWrite (LED_GPIO, HIGH);
    display.setCursor(0,0);
    display.fillRect(0, 0, SCREEN_WIDTH, 9, BLACK);
    display.println("Connecting to WiFi..");
    display.display();
    delay(125);
    digitalWrite (LED_GPIO, LOW);
    display.setCursor(0,0);
    display.fillRect(0, 0, SCREEN_WIDTH, 9, BLACK);
    display.println("Connecting to WiFi...");
    display.display();
    delay(125);
  }
 
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Connected to the WiFinetwork");
  display.display();
  delay(1000);
}

void esp_connect_mqtt(void){
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  int mqtt_count=0;

  while (!client.connected()) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Connecting to MQTT...");
    display.display();
    
    mqtt_count++;

    if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
      display.setCursor(0,20);
      display.println("Connected!");
      display.display();
      // client.publish("modelParam","hello world");
    } else {
      display.setCursor(0,30);
      display.println("failed, rc=" + String(client.state()));
      display.setCursor(0,50);
      display.println("Try again in 2 seconds");
      display.display();
      if(mqtt_count == 10){
        break;
      }
      delay(2000);
    }
  }
  Serial.println(client.subscribe("modelParam"));
}

void setup(void){
  Serial.begin(9600);

  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(0, address);   //Setting the address at which we will receive the data
  radio.setPALevel(RF24_PA_MIN);       //You can set this as minimum or maximum depending on the distance between the transmitter and receiver.
  radio.startListening();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  WiFi.mode(WIFI_STA);
  esp_connect_wifi();
  ThingSpeak.begin(mqttclient);
  esp_connect_mqtt();

  RTC_DATA_ATTR data_queue = xQueueCreate(20, sizeof(Data_t));
  if (data_queue != NULL)
  {
    Serial.println("Data_queue created");
    display.setCursor(0,0);
    display.println("Data_queue created");
    display.display();
  }

  MQTT_semaphore = xSemaphoreCreateMutex();
  if (MQTT_semaphore != NULL)
  {
    display.setCursor(0,10);
    display.println("MQTT_semaph created");
    display.display();
  }

  delay(1000);
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Waiting data RF...");
  display.display();

  xTaskCreatePinnedToCore(RevData_task, "RevData_task", 1024 * 4, NULL, 2, &RevData_task_handle, tskNO_AFFINITY);

}

void loop(void){

}

void Display_task(void *pvParameters){
  Data_t time_data = {};
  Serial.println(pcTaskGetName(NULL));
  while(1){

    time_data.Time_year = data_full.Time_year;
    time_data.Time_month = data_full.Time_month;
    time_data.Time_day = data_full.Time_day;
    time_data.Time_hour = data_full.Time_hour;
    time_data.Time_min = data_full.Time_min;
    time_data.Time_sec = data_full.Time_sec;

    memset(time_buff, 0, 64);
    sprintf(time_buff, "%02d/%02d/%02d %02d:%02d:%02d", 
    time_data.Time_day,
    time_data.Time_month,
    time_data.Time_year,
    time_data.Time_hour,
    time_data.Time_min,
    time_data.Time_sec);

    if(xSemaphoreTake(MQTT_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      display.clearDisplay();
      display.setCursor(0,0);
      display.print(String(time_buff));
      display.setCursor(0,11);
      display.println("Env-Lux: " + String(data_full.Env_Lux));
      display.setCursor(100,11);
      display.println(String(mqtt_check)+ " " +String(http_check));
      
      display.setCursor(0,22);
      display.println("Env-T/H: " + String(data_full.Env_temp)+"/"+String(data_full.Env_Humi));
      display.setCursor(0,33);
      display.println("Soi-T/H: " + String(data_full.Soil_temp)+"/"+String(data_full.Soil_humi));
      display.setCursor(0,44);
      display.println("Soi-pH: " + String(data_full.Soil_pH));
      display.setCursor(0,55);
      display.println("NPK:" + String(data_full.Soil_Nito)+"/"+String(data_full.Soil_Phosp)+"/"+String(data_full.Soil_Kali));
      display.display();
      xSemaphoreGive(MQTT_semaphore);
    }
    vTaskDelete(NULL);
  }
}

void MQTT_task(void *pvParameters){
  
  // Data_t mqtt_data = {};
  Data_t data_temp1;
  Data_t data_temp2;
  while(1){
    if(uxQueueMessagesWaiting(data_queue) >= 2){
      if (xQueueReceive(data_queue, &data_temp2, portMAX_DELAY) == pdPASS){
        Serial.print("MQTT data waiting to read ");
        Serial.print(uxQueueMessagesWaiting(data_queue));
        Serial.print(", Available space ");
        Serial.println(uxQueueSpacesAvailable(data_queue));
        // delay(1010);
        if (xQueueReceive(data_queue, &data_temp1, portMAX_DELAY) == pdPASS){
          Serial.print("MQTT data waiting to read ");
          Serial.print(uxQueueMessagesWaiting(data_queue));
          Serial.print(", Available space ");
          Serial.println(uxQueueSpacesAvailable(data_queue));
          Serial.println(pcTaskGetName(NULL));

          if(data_temp1.index == 1){
            data_full.Env_Lux = data_temp2.Soil_pH;
            data_full.Env_Humi = data_temp2.Soil_humi;
            data_full.Env_temp = data_temp2.Soil_temp;
            data_full.Soil_temp = data_temp1.Soil_temp;
            data_full.Soil_humi = data_temp1.Soil_humi;
            data_full.Soil_pH = data_temp1.Soil_pH;
            data_full.Soil_Nito = data_temp1.Soil_Nito;
            data_full.Soil_Phosp = data_temp1.Soil_Phosp;
            data_full.Soil_Kali = data_temp1.Soil_Kali;
            data_full.Time_year = data_temp1.Time_year;
            data_full.Time_month = data_temp1.Time_month;
            data_full.Time_day = data_temp1.Time_day;
            data_full.Time_hour = data_temp1.Time_hour;
            data_full.Time_min = data_temp1.Time_min;
            data_full.Time_sec = data_temp1.Time_sec;
          } else {
            data_full.Env_Lux = data_temp1.Soil_pH;
            data_full.Env_Humi = data_temp1.Soil_humi;
            data_full.Env_temp = data_temp1.Soil_temp;
            data_full.Soil_temp = data_temp2.Soil_temp;
            data_full.Soil_humi = data_temp2.Soil_humi;
            data_full.Soil_pH = data_temp2.Soil_pH;
            data_full.Soil_Nito = data_temp2.Soil_Nito;
            data_full.Soil_Phosp = data_temp2.Soil_Phosp;
            data_full.Soil_Kali = data_temp2.Soil_Kali;
            data_full.Time_year = data_temp2.Time_year;
            data_full.Time_month = data_temp2.Time_month;
            data_full.Time_day = data_temp2.Time_day;
            data_full.Time_hour = data_temp2.Time_hour;
            data_full.Time_min = data_temp2.Time_min;
            data_full.Time_sec = data_temp2.Time_sec;
          }

          WORD_ALIGNED_ATTR char mqttMessage[512];
          sprintf(mqttMessage, "{\n\t\"Time_real_Date\":\"%02d/%02d/%02d %02d:%02d:%02d\",\n\t\"Soil_temp\":%.2f,\n\t\"Soil_humi\":%.2f,\n\t\"Soil_pH\":%.2f,\n\t\"Soil_Nito\":%.2f,\n\t\"Soil_Kali\":%.2f,\n\t\"Soil_Phosp\":%.2f,\n\t\"Env_temp\":%.2f,\n\t\"Env_Humi\":%.2f,\n\t\"Env_Lux\":%.2f\n}",
                  data_full.Time_day,
                  data_full.Time_month,
                  data_full.Time_year,
                  data_full.Time_hour,
                  data_full.Time_min,
                  data_full.Time_sec,
                  data_full.Soil_temp,
                  data_full.Soil_humi,
                  data_full.Soil_pH,
                  data_full.Soil_Nito,
                  data_full.Soil_Kali,
                  data_full.Soil_Phosp,
                  data_full.Env_temp,
                  data_full.Env_Humi,
                  data_full.Env_Lux
                );
          Serial.println(mqttMessage);
          Serial.println("-----------------------");
          Serial.println(mqttMessage_check);

          // String number1, number2, number3, number4, number5, number6, number7, number8;

          // dtostr(data_full.Env_Lux, 2, 0, number1);
          // dtostr(data_full.Env_temp, 2, 0, number2);
          // dtostr(data_full.Env_Humi, 2, 0, number3);
          // dtostr(data_full.Soil_humi, 2, 0, number4);
          // dtostr(data_full.Soil_pH, 2, 0, number5);
          // dtostr(data_full.Soil_Nito, 2, 0, number6);
          // dtostr(data_full.Soil_Phosp, 2, 0, number7);
          // dtostr(data_full.Soil_Kali, 2, 0, number8);

          ThingSpeak.setField(1, data_full.Env_Lux);
          ThingSpeak.setField(2, data_full.Env_temp);
          ThingSpeak.setField(3, data_full.Env_Humi);
          ThingSpeak.setField(4, data_full.Soil_humi);
          ThingSpeak.setField(5, data_full.Soil_pH);
          ThingSpeak.setField(6, data_full.Soil_Nito);
          ThingSpeak.setField(7, data_full.Soil_Phosp);
          ThingSpeak.setField(8, data_full.Soil_Kali);

          // if(xSemaphoreTake(MQTT_semaphore, portTICK_PERIOD_MS) == pdTRUE){
            mqtt_check = client.publish("modelParam",mqttMessage);
            Serial.println(mqtt_check);

            int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
            if(x == 200){
              Serial.println("Channel update successful.");
              http_check = 1;
            }
            else{
              Serial.println("Problem updating channel. HTTP error code " + String(x));
              http_check = 0;
            }
            // while(!strcmp(mqttMessage, mqttMessage_check)){
            // client.publish("modelParam",mqttMessage);
            // }
          //   xSemaphoreGive(MQTT_semaphore);
          // }

          xTaskCreatePinnedToCore(Display_task, "Display_task", 1024 * 4, NULL, 8, &Display_task_handle, tskNO_AFFINITY);
        }
      }
    }
    vTaskDelete(NULL);
  }
}

void RevData_task(void *pvParameters){
  Serial.println(pcTaskGetName(NULL));
  while(1){
    if (radio.available()){ 
      radio.read(&SensorData, sizeof(Data_t));
      if (xQueueSendToBack(data_queue, (void *)&SensorData, 1000/portMAX_DELAY) == pdTRUE  ){
        Serial.print("MQTT data waiting to read ");
        Serial.print(uxQueueMessagesWaiting(data_queue));
        Serial.print(", Available space ");
        Serial.println(uxQueueSpacesAvailable(data_queue));
      }
    } else {
      xTaskCreatePinnedToCore(MQTT_task, "MQTT_task", 1024 * 4, NULL, 4, &MQTT_task_handle, tskNO_AFFINITY);
    }
    vTaskDelay(10/portTICK_PERIOD_MS);
  }
}