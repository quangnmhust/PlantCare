#include <PubSubClient.h>
#include <ThingSpeak.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <BH1750.h>
#include<SHT25.h>

#include "time.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include <WiFi.h>
#include <Arduino.h>
#include "String.h"
#include "EEPROM.h"

#define uS_TO_M_FACTOR 1000000
#define TIME_TO_SLEEP 10
#define Period_minute_time 0.5

#define LENGTH(x) (strlen(x) + 1)
#define EEPROM_SIZE 200
#define WiFi_rst 0

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define ADDRESS 0x02
#define FUNCTION_CODE 0x03
#define INITIAL_ADDRESS_HI 0x00
#define INITIAL_ADDRESS_LO 0x00
#define DATA_LENGTH_HI 0x00
#define DATA_LENGTH_LO 0x0A
#define CHECK_CODE_LO 0xC5
#define CHECK_CODE_HI 0xFE
#define NUMBER_BYTES_RESPONES 25

// const char* ssid = "Tdnt";
// const char* password =  "nhatthang642002";
String ssid = "";
String pss =  "";
const int myChannelNumber = 2515584;
const char * myWriteAPIKey = "8TGOAVM92D494KS1";
const char* mqttServer = "sanslab.ddns.net";
const int mqttPort = 1883;
const char* mqttUser = "admin";
const char* mqttPassword = "123";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 6*3600+5;
const int   daylightOffset_sec = 3600;

TaskHandle_t RS485_task_handle;
TaskHandle_t SHT25_task_handle;
TaskHandle_t BH1750_task_handle;
TaskHandle_t DS3231_task_handle;
TaskHandle_t MQTT_task_handle;
TaskHandle_t Display_task_handle;
TaskHandle_t Displaytime_task_handle;
TaskHandle_t Control_task_handle;

static StaticSemaphore_t xSemaphoreBuffer;
SemaphoreHandle_t I2C_semaphore = NULL;
SemaphoreHandle_t MQTT_semaphore = NULL;

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

volatile Data_t SensorData;

BH1750 lightMeter;
RTC_DS3231 rtc;
SHT25 H_Sens;

// WiFiClient  httpclient;
WiFiClient mqttclient;
PubSubClient client(mqttclient);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const byte request[] = {ADDRESS, FUNCTION_CODE, INITIAL_ADDRESS_HI, INITIAL_ADDRESS_LO, DATA_LENGTH_HI, DATA_LENGTH_LO, CHECK_CODE_LO, CHECK_CODE_HI};
byte sensorResponse[NUMBER_BYTES_RESPONES];
char SD_Frame[1024];
char MQTT_Frame[1024];
char time_buff[20];
RTC_DATA_ATTR int errorCount = 0;
// String ssid, pss;

unsigned long rst_millis;

float convertBytesToFloat(byte hi, byte low){
  int intVal = (hi << 8) | low;
  //Serial.println(intVal);
  return (float)intVal/10.0;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

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

void getSoilSensorParameter(){
  float soilHumi, soilTemp, soilPH, soilN,soilP,soilK;
  // float soilConduct;

  soilHumi = convertBytesToFloat(sensorResponse[3], sensorResponse[4]);
  soilTemp = convertBytesToFloat(sensorResponse[5], sensorResponse[6]);
  // soilConduct = convertBytesToFloat(sensorResponse[7], sensorResponse[8]);
  soilPH = convertBytesToFloat(sensorResponse[9], sensorResponse[10]);
  soilN = convertBytesToFloat(sensorResponse[11], sensorResponse[12]);
  soilP = convertBytesToFloat(sensorResponse[13], sensorResponse[14]);
  soilK = convertBytesToFloat(sensorResponse[15], sensorResponse[16]);

  SensorData.Soil_temp = soilTemp;
  SensorData.Soil_humi = soilHumi;
  SensorData.Soil_pH = soilPH;
  SensorData.Soil_Nito = soilN;
  SensorData.Soil_Phosp = soilP;
  SensorData.Soil_Kali = soilK;

}
///////////////////////////////////////////////////////////////////////////////////////////

void DS3231_task(void *pvParameters); //Take time task
void Displaytime_task(void *pvParameters);
void MQTT_task(void *pvParameters); //MQTT Task
void RS485_task(void *pvParameters); // Soil parameters
void SHT25_task(void *pvParameters); // Temp, humi parameters
void BH1750_task(void *pvParameters); // Lux parameter
void Control_task(void *pvParameters);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
  }
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  I2C_semaphore = xSemaphoreCreateCountingStatic( 4, 4, &xSemaphoreBuffer );
  if (I2C_semaphore != NULL)
  {
    display.setCursor(0,0);
    display.println("I2C_semaphore created");
    display.setCursor(0,10);
    display.println(4);
    display.display();
  }
  MQTT_semaphore = xSemaphoreCreateMutex();
  if (MQTT_semaphore != NULL)
  {
    display.setCursor(0,20);
    display.println("MQTT_semaphore created");
    display.display();
    delay(1000);
    display.clearDisplay();
  }
  // WiFi.begin(ssid, password);
 
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.println("Connecting to WiFi..");
  // }
 
  // Serial.println("Connected to the WiFi network");
 
  // client.setServer(mqttServer, mqttPort);
 
  // while (!client.connected()) {
  //   Serial.println("Connecting to MQTT...");
 
  //   if (client.connect("ESP32Client", mqttUser, mqttPassword )) {
 
  //     Serial.println("connected");
 
  //   } else {
 
  //     Serial.print("failed with state ");
  //     Serial.println(client.state());
  //     delay(2000);
 
  //   }
  // }

  // if(!SD.begin()){
  //     Serial.println("Card Mount Failed");
  // } else {
  //   uint8_t cardType = SD.cardType();

  //   if(cardType == CARD_NONE){
  //     Serial.println("No SD card attached");
  //   }

  //   Serial.print("SD Card Type: ");
  //   if(cardType == CARD_MMC){
  //       Serial.println("MMC");
  //   } else if(cardType == CARD_SD){
  //       Serial.println("SDSC");
  //   } else if(cardType == CARD_SDHC){
  //       Serial.println("SDHC");
  //   } else {
  //       Serial.println("UNKNOWN");
  //   }

  //   uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  //   Serial.printf("SD Card Size: %lluMB\n", cardSize);
  // }

  if (! rtc.begin()) {

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Couldn't find RTC");
    display.display();
  } else {
   if (rtc.lostPower()) {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("RTC lost power, let's set the time!");
      // WiFi.begin(ssid, password);
      
      // Init and get the time
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

      struct tm timeinfo;
      if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        // return;
      }
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    }
  }

  H_Sens.begin();
  // lightMeter.begin();
  // writeFile(SD, "/data.csv", "Date,Month,Year,Hour,Min,Sec,Lux,E_Tem,E_Hum,S_Tem,S_Hum,S_pH,S_Ni,S_Ph,S_Ka\n");
  // ThingSpeak.begin(mqttclient);

  xTaskCreatePinnedToCore(Control_task, "Control_task", 1024 * 4, NULL, 1, &Control_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(DS3231_task, "DS3231_Task", 1024 * 4, NULL, 3, &DS3231_task_handle, tskNO_AFFINITY);
  
   
}
 
void loop() {
  rst_millis = millis();
  while (digitalRead(WiFi_rst) == LOW) {
  }
  if (millis() - rst_millis >= 3000) {
    Serial.println("Reseting the WiFi credentials");
    writeStringToFlash("", 0); // Reset the SSID
    writeStringToFlash("", 40); // Reset the Password
    Serial.println("Wifi credentials erased");
    Serial.println("Restarting the ESP");
    delay(500);
    ESP.restart();
  }
  client.loop();
}

//Task

void RS485_task(void *pvParameters){
  Serial2.begin(4800);
  while(1){
    Serial2.write(request, sizeof(request));
    vTaskDelay(10/portTICK_RATE_MS);
    unsigned long startTime = millis();
    while (Serial2.available() < NUMBER_BYTES_RESPONES && millis() - startTime < 1000)
    {
      vTaskDelay(1/portTICK_RATE_MS);
    }

    if (Serial2.available() >= NUMBER_BYTES_RESPONES){
      byte index = 0;
      while (Serial2.available() && index < NUMBER_BYTES_RESPONES){
        sensorResponse[index] = Serial2.read();
        index++;
      }
      getSoilSensorParameter();
    } else {
      Serial.println("Sensor timeout or incomplete frame");
    }
    vTaskDelete(NULL);
  }
}

void SHT25_task(void *pvParameters){
  Serial.println(pcTaskGetName(NULL));
  while(1){
    if (xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      SensorData.Env_temp = H_Sens.getTemperature();
      SensorData.Env_Humi = H_Sens.getHumidity();
      Serial.println(SensorData.Env_temp);
      Serial.println(SensorData.Env_Humi);
      xSemaphoreGive(I2C_semaphore);
    }
    vTaskDelete(NULL);
  }
}

void BH1750_task(void *pvParameters){
  while(1){
    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      SensorData.Env_Lux = lightMeter.readLightLevel();
      xSemaphoreGive(I2C_semaphore);
    }
    vTaskDelay(Period_minute_time*60000/portTICK_PERIOD_MS);
  }
}

void MQTT_task(void *pvParameters){
  while(1){
    if(xSemaphoreTake(MQTT_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      memset(MQTT_Frame, 0, 1024);
      sprintf(MQTT_Frame, "{\n\t\"Time_real_Date\":\"%02d/%02d/%02d %02d:%02d:%02d\",\n\t\"Soil_temp\":%.2f,\n\t\"Soil_humi\":%.2f,\n\t\"Soil_pH\":%.2f,\n\t\"Soil_Nito\":%.2f,\n\t\"Soil_Kali\":%.2f,\n\t\"Soil_Phosp\":%.2f,\n\t\"Env_temp\":%.2f,\n\t\"Env_Humi\":%.2f,\n\t\"Env_Lux\":%.2f\n}",
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
              SensorData.Soil_Kali,
              SensorData.Soil_Phosp,
              SensorData.Env_temp,
              SensorData.Env_Humi,
              SensorData.Env_Lux 
      );

      Serial.print("MQTT: "); 
      Serial.println(MQTT_Frame);
      xSemaphoreGive(MQTT_semaphore);
    }
    vTaskDelete(NULL);
  }
}

void Displaytime_task(void *pvParameters){
  while(1){
    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){;
      display.fillRect(0, 0, SCREEN_WIDTH, 9, BLACK);
      display.setCursor(0,0);
      display.print(time_buff);
      display.display();
      // Serial.println(time_buff);
      xSemaphoreGive(I2C_semaphore);
    }
    vTaskDelete(NULL);
  }
}

void DS3231_task(void *pvParameters){
  // Serial.println(pcTaskGetName(NULL));
  while(1){
    // Serial.println(pcTaskGetName(NULL));
    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      DateTime now = rtc.now();
      
      SensorData.Time_year = now.year();
      SensorData.Time_month = now.month();
      SensorData.Time_day = now.day();
      SensorData.Time_hour = now.hour();
      SensorData.Time_min = now.minute();
      SensorData.Time_sec = now.second();

      memset(time_buff, 0, 512);
      sprintf(time_buff, "%02d/%02d/%02d %02d:%02d:%02d", 
      SensorData.Time_day,
      SensorData.Time_month,
      SensorData.Time_year,
      SensorData.Time_hour,
      SensorData.Time_min,
      SensorData.Time_sec);

      // Serial.println(time_buff);

      xSemaphoreGive(I2C_semaphore);
    }
    xTaskCreatePinnedToCore(Displaytime_task, "Displaytime_task", 1024 * 4, NULL, 4, &Displaytime_task_handle, tskNO_AFFINITY);
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
} 

void Display_task(void *pvParameters){
  while(1){
    vTaskDelay(2000/portTICK_PERIOD_MS);
    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      display.clearDisplay();
      memset(SD_Frame, 0, 1024);

      // "Date,Month,Year,Hour,Min,Sec,Lux,E_Tem,E_Hum,S_Tem,S_Hum,S_pH,S_Ni,S_Ph,S_Ka\n"
      // sprintf(SD_Frame, "%02d,%02d,%02d,%02d,%02d,%02d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
      sprintf(SD_Frame, "{\n\t\"Time_real_Date\":\"%02d/%02d/%02d %02d:%02d:%02d\",\n\t\"Soil_temp\":%.2f,\n\t\"Soil_humi\":%.2f,\n\t\"Soil_pH\":%.2f,\n\t\"Soil_Nito\":%.2f,\n\t\"Soil_Kali\":%.2f,\n\t\"Soil_Phosp\":%.2f,\n\t\"Env_temp\":%.2f,\n\t\"Env_Humi\":%.2f,\n\t\"Env_Lux\":%.2f\n}", 
              SensorData.Time_day,
              SensorData.Time_month,
              SensorData.Time_year,
              SensorData.Time_hour,
              SensorData.Time_min,
              SensorData.Time_sec,
              SensorData.Env_Lux,
              SensorData.Env_temp,
              SensorData.Env_Humi,
              SensorData.Soil_temp,
              SensorData.Soil_humi,
              SensorData.Soil_pH,
              SensorData.Soil_Nito,
              SensorData.Soil_Phosp,
              SensorData.Soil_Kali
      );

      Serial.print("SD: "); 
      Serial.println(SD_Frame);

      // xTaskCreatePinnedToCore(MQTT_task, "MQTT_task", 1024 * 4, NULL, 6, &MQTT_task_handle, tskNO_AFFINITY);

      int count_d=1;
      while(count_d<3){
        display.fillRect(0, 8, SCREEN_WIDTH, 60, BLACK);
        display.setCursor(0,11);
        display.println("Env-Lux: " + String(SensorData.Env_Lux));
        display.setCursor(0,22);
        display.println("Env-T/H: " + String(SensorData.Env_temp)+"/"+String(SensorData.Env_Humi));
        display.setCursor(0,33);
        display.println("Soi-T/H: " + String(SensorData.Soil_temp)+"/"+String(SensorData.Soil_humi));
        display.setCursor(0,44);
        display.println("Soi-pH: " + String(SensorData.Soil_pH));
        display.setCursor(0,55);
        display.println("NPK:" + String(SensorData.Soil_Nito)+"/"+String(SensorData.Soil_Phosp)+"/"+String(SensorData.Soil_Kali));
        display.display();
        count_d++;
      }

      Serial.println("-----------------------------------------------------------");
      xSemaphoreGive(I2C_semaphore);
    }
    vTaskDelete(NULL);
  }
}

void Control_task(void *pvParameters){
  while(1){
    vTaskDelay(2000/portTICK_PERIOD_MS);
  // xTaskCreatePinnedToCore(RS485_task, "RS485_Task", 1024 * 4, NULL, 5, &RS485_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(SHT25_task, "SHT25_Task", 1024 * 4, NULL, 5, &SHT25_task_handle, tskNO_AFFINITY);
  // xTaskCreatePinnedToCore(BH1750_task, "BH1750_Task", 1024 * 4, NULL, 5, &BH1750_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(Display_task, "Display_task", 1024 * 4, NULL, 6, &Display_task_handle, tskNO_AFFINITY);

  vTaskDelay(Period_minute_time*60000-2000/portTICK_PERIOD_MS);
  }
}
