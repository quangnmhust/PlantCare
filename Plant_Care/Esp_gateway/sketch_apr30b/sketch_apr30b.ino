#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

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

Data_f_t data_full;

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

QueueHandle_t data_queue = NULL;

RF24 radio(4, 5, 18, 19, 23); //CE-CS-SCK-miso-mosi

void setup(void){
  Serial.begin(115200);
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(0, address);   //Setting the address at which we will receive the data
  radio.setPALevel(RF24_PA_MIN);       //You can set this as minimum or maximum depending on the distance between the transmitter and receiver.
  radio.startListening();
  data_queue = xQueueCreate(20, sizeof(Data_t));
  if (data_queue != NULL)
  {
    Serial.println("Data_queue created");
  }
}

void loop(void){
 if (radio.available()){  
    radio.read(&SensorData, sizeof(Data_t));
    if (xQueueSendToBack(data_queue, (void *)&SensorData, 1000/portMAX_DELAY) == pdTRUE  ){
			Serial.print("MQTT data waiting to read ");
      Serial.print(uxQueueMessagesWaiting(data_queue));
      Serial.print(", Available space ");
      Serial.println(uxQueueSpacesAvailable(data_queue));
		}
    delay(10);
 }
 else{
  //Serial.println("No radio available");
 }

  delay(1000);
  Data_t data_temp1;
  Data_t data_temp2;


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
                data_full.Soil_Phosp,
                data_full.Soil_Kali,
                data_full.Env_temp,
                data_full.Env_Humi,
                data_full.Env_Lux
              );
        Serial.println(mqttMessage);
      }
    }
  }
}