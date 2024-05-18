#include "config_ver3.h"
#include "Adafruit_SHT4x.h"
#include "RTClib.h"
#include <ESP8266WiFi.h>
#include <BH1750.h>
#include <Wire.h>
#include <espnow.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <ThingSpeak.h>

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
RTC_DS3231 rtc;
BH1750 lightMeter;
WiFiClient mqttclient;
PubSubClient client(mqttclient);

volatile Data_t SensorData;
SoftwareSerial mod(14, 12);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  // Serial.setTimeout(2000);
  mod.begin(4800);
  // mod.setTimeout(2000);
  Wire.begin();

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  if (rtc.lostPower()) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED){
      delay(125);
    }
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      rtc.adjust(DateTime(0, 0, 0, 0, 0, 0));
    }
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    WiFi.disconnect();
  }

  Serial.println("Adafruit SHT4x test");
  if (! sht4.begin()) {
    Serial.println("Couldn't find SHT4x");
    while (1) delay(1);
  }
  Serial.println("Found SHT4x sensor");
  Serial.print("Serial number 0x");
  Serial.println(sht4.readSerial(), HEX);
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  lightMeter.begin();

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(mqttclient);

  // // Init ESP-NOW
  // if (esp_now_init() != 0) {
  //   Serial.println("Error initializing ESP-NOW");
  //   return;
  // }
  // // Once ESPNow is successfully Init, we will register for Send CB to
  // // get the status of Trasnmitted packet
  // esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  // esp_now_register_send_cb(OnDataSent);
  
  // // Register peer
  // esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_CONTROLLER, 1, NULL, 0);

  /*--------------------------------------------Program--------------------------------------------*/
  lockMutex();
    DateTime now = rtc.now();
    SensorData.Time_year = now.year();
    SensorData.Time_month = now.month();
    SensorData.Time_day = now.day();
    SensorData.Time_hour = now.hour();
    SensorData.Time_min = now.minute();
    SensorData.Time_sec = now.second();
    
  unlockMutex();

  lockMutex();
    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
    SensorData.Env_temp = temp.temperature;
    SensorData.Env_Humi = humidity.relative_humidity;
  unlockMutex();

  lockMutex();
    SensorData.Env_Lux = lightMeter.readLightLevel();
  unlockMutex();

  lockMutex();
    mod.write(request, sizeof(request));
    delay(10);
  
    unsigned long starttimers485 = millis();
    while (mod.available() < NUMBER_BYTES_RESPONES && millis() - starttimers485 < 1000)
    {
      delay(1);
    }
    if (mod.available() >= NUMBER_BYTES_RESPONES)
    {
      byte index = 0;
      while (mod.available() && index < NUMBER_BYTES_RESPONES)
      {
        sensorResponse[index] = mod.read();
        Serial.print(sensorResponse[index], HEX); // Print the received byte in HEX format
        Serial.print(" ");
        index++;
      }
      SensorData.Soil_humi = convertBytesToFloat(sensorResponse[3], sensorResponse[4]);
      SensorData.Soil_temp = convertBytesToFloat(sensorResponse[5], sensorResponse[6]);
      SensorData.Soil_pH = convertBytesToFloat(sensorResponse[9], sensorResponse[10]);
      SensorData.Soil_Nito = convertBytesToFloat(sensorResponse[11], sensorResponse[12]);
      SensorData.Soil_Phosp = convertBytesToFloat(sensorResponse[13], sensorResponse[14]);
      SensorData.Soil_Kali = convertBytesToFloat(sensorResponse[15], sensorResponse[16]);
    }
    else
    {
      Serial.println("Sensor timeout or incomplete frame");
    }
  unlockMutex();
  char mqttMessage[512];
  sprintf(mqttMessage, "{\n\t\"Time_real_Date\":\"%02d/%02d/%02d %02d:%02d:%02d\",\n\t\"Soil_temp\":%.2f,\n\t\"Soil_humi\":%.2f,\n\t\"Soil_pH\":%.2f,\n\t\"Soil_Nito\":%.2f,\n\t\"Soil_Kali\":%.2f,\n\t\"Soil_Phosp\":%.2f,\n\t\"Env_temp\":%.2f,\n\t\"Env_Humi\":%.2f,\n\t\"Env_Lux\":%.2f\n}",
          SensorData.Time_day,
          SensorData.Time_month,
          SensorData.Time_year,
          SensorData.Time_hour,
          SensorData.Time_min,
          SensorData.Time_sec,
          SensorData.Soil_temp,
          SensorData.Soil_humi,
          SensorData.Soil_pH,
          SensorData.Soil_Nito,
          SensorData.Soil_Phosp,
          SensorData.Soil_Kali,
          SensorData.Env_temp,
          SensorData.Env_Humi,
          SensorData.Env_Lux);
  // Serial.println(mqttMessage);
  // esp_now_send(broadcastAddress, (uint8_t *) &SensorData, sizeof(Data_t));

  // Connect or reconnect to WiFi
  if(WiFi.status() != WL_CONNECTED){
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, password);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);     
    } 
    Serial.println("\nConnected.");
  }
  // set the fields with the values
  ThingSpeak.setField(1, SensorData.Env_temp);
  ThingSpeak.setField(2, SensorData.Env_Humi);
  ThingSpeak.setField(3, SensorData.Env_Lux);
  ThingSpeak.setField(4, SensorData.Soil_humi);
  ThingSpeak.setField(5, SensorData.Soil_pH);
  ThingSpeak.setField(6, SensorData.Soil_Nito);
  ThingSpeak.setField(7, SensorData.Soil_Phosp);
  ThingSpeak.setField(8, SensorData.Soil_Kali);
  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
    delay(5000);
    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  }
  /*--------------------------------------------Program--------------------------------------------*/
  Serial.println("I'm going into deep sleep mode for 30 seconds");
  ESP.deepSleep(40e7);
}

void loop() {
}

float convertBytesToFloat(byte hi, byte low) {
  int intVal = (hi << 8) | low;
  return (float)intVal / 10.0;
}

// void callback(char *topic, byte *payload, unsigned int length){
//   Serial.print("Topic: ");
//   Serial.println(String(topic));
//   Serial.print("Message: ");
//   for (int i = 0; i < length; i++) {
//     Serial.print((char)payload[i]);
//   }
// }

// Callback when data is sent
// void OnDataSent(uint8_t *mac_addr, u8 sendStatus) {
//   Serial.print("Last Packet Send Status: ");
//   Serial.println(sendStatus == 0 ? "Delivery Success" : "Delivery Fail");
//   Serial.println("I'm going into deep sleep mode for 5 mins");
//   ESP.deepSleep(30e7);
// }

void lockMutex() {
  while (mutexFlag) {
  }
  mutexFlag = true;
}

void unlockMutex() {
  mutexFlag = false;
}