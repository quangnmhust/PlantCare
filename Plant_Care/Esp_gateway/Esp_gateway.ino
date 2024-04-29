#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
char time_buff[64];
RF24 radio(4, 5, 18, 19, 23); //CE-CS-SCK-miso-mosi
const uint64_t addr = 0xE8E8F0F0E1LL;

typedef struct Data_manager{
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
} Data_t;

Data_t recvData;

void setup(void){
 Serial.begin(115200);
 pinMode(2, OUTPUT);
 if(!radio.begin()){
    Serial.println("RF Failed");
  }else{
    Serial.println("RF");
  }
 radio.setChannel(2);
//  radio.setPayloadSize(24);
 radio.setDataRate(RF24_250KBPS);
 radio.openReadingPipe(1,addr);
 radio.startListening();
}

void loop(void){
    digitalWrite(2, LOW);
 if (radio.available()){
    radio.read(&recvData, sizeof(Data_t));
    digitalWrite(2, HIGH);
    memset(time_buff, 0, 64);
    sprintf(time_buff, "%02d/%02d/%02d %02d:%02d:%02d", 
    recvData.Time_day,
    recvData.Time_month,
    recvData.Time_year,
    recvData.Time_hour,
    recvData.Time_min,
    recvData.Time_sec);
    // Serial.println("Time: " + String(time_buff));
    // Serial.println("Env-Lux: " + String(recvData.Env_Lux));
    // Serial.println("Env-T/H: " + String(recvData.Env_temp)+"/"+String(recvData.Env_Humi));
    // Serial.println("Soi-T/H: " + String(recvData.Soil_temp)+"/"+String(recvData.Soil_humi));
    // Serial.println("Soi-pH: " + String(recvData.Soil_pH));
    // Serial.println("NPK:" + String(recvData.Soil_Nito)+"/"+String(recvData.Soil_Phosp)+"/"+String(recvData.Soil_Kali));
    WORD_ALIGNED_ATTR char mqttMessage[512];
    sprintf(mqttMessage, "{\n\t\"Time_real_Date\":\"%02d/%02d/%02d %02d:%02d:%02d\",\n\t\"Soil_temp\":%.2f,\n\t\"Soil_humi\":%.2f,\n\t\"Soil_pH\":%.2f,\n\t\"Soil_Nito\":%.2f,\n\t\"Soil_Kali\":%.2f,\n\t\"Soil_Phosp\":%.2f,\n\t\"Env_temp\":%.2f,\n\t\"Env_Humi\":%.2f,\n\t\"Env_Lux\":%.2f\n}",
            recvData.Time_day,
            recvData.Time_month,
            recvData.Time_year,
            recvData.Time_hour,
            recvData.Time_min,
            recvData.Time_sec,
            recvData.Soil_temp,
            recvData.Soil_humi,
            recvData.Soil_pH,
            recvData.Soil_Nito,
            recvData.Soil_Phosp,
            recvData.Soil_Kali,
            recvData.Env_temp,
            recvData.Env_Humi,
            recvData.Env_Lux
          );
    Serial.println(mqttMessage);
    delay(10);
 }
 else{
  //Serial.println("No radio available");
 }
}