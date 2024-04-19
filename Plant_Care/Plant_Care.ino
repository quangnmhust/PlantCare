#include <RTClib.h>
#include "time.h"
#include <BH1750.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include <WiFi.h>
#include <SoftwareSerial.h>
#include <Arduino.h>
#include "Wire.h"
#include "String.h"

BH1750 lightMeter;
RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

const char* ssid     = "Tom Bi";
const char* password = "TBH123456";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 6*3600+5;
const int   daylightOffset_sec = 3600;

// SHT25 I2C address is 0x40(64)
#define SHT25_ADDR    0x40
#define is_RH         0x01
#define is_TEMP       0x00
//Commands
#define T_NO_HOLD     0xF3 //trigger T measurement with no hold master
#define RH_NO_HOLD    0xF5 //trigger RH measurement with no hold master
#define W_UREG        0xE6 //write user registers
#define R_UREG        0xE7 //read user registers
#define SOFT_RESET    0xFE //soft reset

//Define for RS485 sensor
#define ADDRESS 0x02
#define FUNCTION_CODE 0x03
#define INITIAL_ADDRESS_HI 0x00
#define INITIAL_ADDRESS_LO 0x00
#define DATA_LENGTH_HI 0x00
#define DATA_LENGTH_LO 0x0A
#define CHECK_CODE_LO 0xC5
#define CHECK_CODE_HI 0xFE
#define NUMBER_BYTES_RESPONES 25

// Request frame for the soil sensor    
const byte request[] = {ADDRESS, FUNCTION_CODE, INITIAL_ADDRESS_HI, INITIAL_ADDRESS_LO, DATA_LENGTH_HI, DATA_LENGTH_LO, CHECK_CODE_LO, CHECK_CODE_HI};
byte sensorResponse[NUMBER_BYTES_RESPONES];

SoftwareSerial mod(17, 16); // Software serial for RS485 communication rx21 tx22

TaskHandle_t RS485_task_handle;
TaskHandle_t SHT25_task_handle;
TaskHandle_t BH1750_task_handle;
TaskHandle_t DS3231_task_handle;
TaskHandle_t SD_task_handle;

SemaphoreHandle_t I2C_mutex = NULL;

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

Data_t SensorData;
// Data_t Data;

float TEMP, RH, S_T, S_RH;
byte T_Delay = 85;  //for 14 bit resolution
byte RH_Delay = 29; //for 12 bit resolution 

//RS485 Function
/////////////////////////////////////////////////////////////////////

float convertBytesToFloat(byte hi, byte low){
  int intVal = (hi << 8) | low;
  //Serial.println(intVal);
  return (float)intVal/10.0;
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

void printSoilParameters(Data_t Data){
  Serial.print("Độ ẩm: ");
  Serial.print(Data.Soil_humi);
  Serial.println(" %RH");

  Serial.print("Nhiệt độ: ");
  Serial.print(Data.Soil_temp);
  Serial.println(" oC");

  // Serial.print("Conductivity: ");
  // Serial.print(soilConduct);
  // Serial.println(" uS/cm");

  Serial.print("pH hiện tại: ");
  Serial.print(Data.Soil_pH);
  Serial.println(" ");

  Serial.print("Nito hiện tại: ");
  Serial.print(Data.Soil_Nito);
  Serial.println(" ppm");

  Serial.print("Photpho hiện tại: ");
  Serial.print(Data.Soil_Phosp);
  Serial.println(" ppm");

  Serial.print("Kali hiện tại: ");
  Serial.print(Data.Soil_Kali);
  Serial.println(" ppm");
}
////////////////////////////////////////////////////////////////////


//SHT25 Function
/////////////////////////////////////////////////////////////////////

char SHT25_resetSensor(void){
  Wire.beginTransmission(SHT25_ADDR);
  Wire.write(SOFT_RESET);
  char error = Wire.endTransmission();
  if(error == 0){
    Serial.println("Reset SHT25");
    vTaskDelay(15/portTICK_PERIOD_MS);    //wait for sensor to reset
    return 1;
    } else {
    return 0;
    }
}

char SHT25_begin(void){
  // Wire.begin();
  if(SHT25_resetSensor()){
    return 1;
  }else{return 0;}
}

char SHT25_readBytes(char CMD, float &value, char length, char param){
  unsigned char data[length];
  Wire.beginTransmission(SHT25_ADDR);
  Wire.write(CMD);
  char error = Wire.endTransmission();
  if(param){
    vTaskDelay(RH_Delay/portTICK_PERIOD_MS);
  }else{
    vTaskDelay(T_Delay/portTICK_PERIOD_MS);
    }

  if(error == 0){
    Wire.requestFrom(SHT25_ADDR, length);
    while (!Wire.available());
    for(char x=0; x<length; x++){
      data[x] = Wire.read();
      x++;
    }
    value = (float)((unsigned int)data[0]*((int)1<<8) + (unsigned int)(data[1]&((int)1<<2)));
    return 1;
  } else {
    return 0;
    }
}

float SHT25_getHumidity(void){
  if(SHT25_readBytes(RH_NO_HOLD, S_RH, 3, is_RH)){
    RH = -6.0 + 125.0*(S_RH/((long)1<<16));
    return RH;
  } else {
    return 0;
    }
}

float SHT25_getTemperature(void){
  if(SHT25_readBytes(T_NO_HOLD, S_T, 3, is_TEMP)){
    TEMP = -46.85 + 175.72*(S_T/((long)1<<16));
    return TEMP;
  } else {
    return 0;
    }
}

void printEnvParameters(Data_t Data){
  Serial.print("Độ ẩm env: ");
  Serial.print(Data.Env_Humi);
  Serial.println(" %RH");

  Serial.print("Nhiệt độ env: ");
  Serial.print(Data.Env_temp);
  Serial.println(" oC");
}
/////////////////////////////////////////////////////////////////////

//SD Function
/////////////////////////////////////////////////////////////////////

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    // File file = fs.open(path, FILE_WRITE);
    File file;
    if (!SD.exists(path)) {//nếu fle này chưa được tạo => tạo ra rồi ghi
      
      file = fs.open(path, FILE_WRITE);
    }
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
        Serial.print("Message appended: ");
        Serial.println(message);
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

int SD_start_check(void){
  if(!SD.begin()){
        Serial.println("Card Mount Failed");
        return 0;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return 0;
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    return 1;
}
//////////////////////////////////////////////////////////////////////////////

//DS3231 Function
/////////////////////////////////////////////////////////////////////
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  
  
  Serial.print(timeinfo.tm_mday);
  Serial.print("/");
  Serial.print(timeinfo.tm_mon + 1);
  Serial.print("/");
  Serial.print(timeinfo.tm_year + 1900);
  Serial.print(" ");
  Serial.print(timeinfo.tm_hour, DEC);
  Serial.print(":");
  Serial.print(timeinfo.tm_min, DEC);
  Serial.print(":");
  Serial.println(timeinfo.tm_sec, DEC);

}
//////////////////////////////////////////////////////////////////////////////


void DS3231_task(void *pvParameters); //Take time task
void SD_task(void *pvParameters); //SD Task

void RS485_task(void *pvParameters); // Soil parameters
void SHT25_task(void *pvParameters); // Temp, humi parameters
void BH1750_task(void *pvParameters); // Lux parameter

void setup() {
  Serial.begin(9600); //init serial with computer
  Wire.begin();
  // SHT25_begin();

  I2C_mutex = xSemaphoreCreateMutex();  //Init I2C mutex
  if (I2C_mutex != NULL)
  {
    Serial.println("I2C_mutex created");
  }

  xTaskCreatePinnedToCore(DS3231_task, "DS3231_Task", 1024 * 4, NULL, 2, &DS3231_task_handle, tskNO_AFFINITY);
  // xTaskCreatePinnedToCore(SD_task, "SD_Task", 1024 * 4, NULL, 2, &SD_task_handle, tskNO_AFFINITY);

  // xTaskCreatePinnedToCore(RS485_task, "RS485_Task", 1024 * 4, NULL, 3, &RS485_task_handle, tskNO_AFFINITY);
  // xTaskCreatePinnedToCore(SHT25_task, "SHT25_Task", 1024 * 4, NULL, 3, &SHT25_task_handle, tskNO_AFFINITY);
  // xTaskCreatePinnedToCore(BH1750_task, "BH1750_Task", 1024 * 4, NULL, 3, &BH1750_task_handle, tskNO_AFFINITY);

}

void loop() {
  // No thing in here because everything run in tasks
}

//Task

void RS485_task(void *pvParameters){
  // mod.begin(4800);    // Initialize software serial communication at 4800 baud rate
  Serial2.begin(4800);

  while(1){
    // mod.write(request, sizeof(request)); // Send the request frame to the soil sensor
    Serial2.write(request, sizeof(request));
    vTaskDelay(10/portTICK_RATE_MS);
    // Wait for the response from the sensor or timeout after 1 second
    unsigned long startTime = millis();
    // while (mod.available() < NUMBER_BYTES_RESPONES && millis() - startTime < 1000)
    while (Serial2.available() < NUMBER_BYTES_RESPONES && millis() - startTime < 1000)
    {
      vTaskDelay(1/portTICK_RATE_MS);
    }
    Serial.println(mod.available());

    // if (mod.available() >= NUMBER_BYTES_RESPONES){
    if (Serial2.available() >= NUMBER_BYTES_RESPONES){
      // Read the response from the sensor
      byte index = 0;
      // while (mod.available() && index < NUMBER_BYTES_RESPONES){
      //   sensorResponse[index] = mod.read();
      while (Serial2.available() && index < NUMBER_BYTES_RESPONES){
        sensorResponse[index] = Serial2.read();
        Serial.print(sensorResponse[index], HEX);
        Serial.print(" ");
        index++;
      }
      Serial.println(" $End of Rx data");
      getSoilSensorParameter();
      printSoilParameters(SensorData);
    } else {
      Serial.println("Sensor timeout or incomplete frame");
    }
    vTaskDelay(1000/portTICK_RATE_MS);
  }
}

void SHT25_task(void *pvParameters){
  
  SHT25_begin();

  while(1){
    Serial.println(pcTaskGetName(NULL));
    if (xSemaphoreTake(I2C_mutex, portTICK_PERIOD_MS) == pdTRUE)
    {
      SensorData.Env_temp = SHT25_getTemperature();
      SensorData.Env_Humi = SHT25_getHumidity();
      printEnvParameters(SensorData);

      xSemaphoreGive(I2C_mutex);
    }
    vTaskDelay(5000/portTICK_PERIOD_MS);
  }
}

void BH1750_task(void *pvParameters){
  
  lightMeter.begin();
  while(1){
    Serial.println(pcTaskGetName(NULL));
    if(xSemaphoreTake(I2C_mutex, portTICK_PERIOD_MS) == pdTRUE){
      SensorData.Env_Lux = lightMeter.readLightLevel();

      Serial.print("Lux env: ");
      Serial.print(SensorData.Env_Lux);
      Serial.println(" lux");

      xSemaphoreGive(I2C_mutex);
    }
    vTaskDelay(5000/portTICK_PERIOD_MS);
  }
}

void DS3231_task(void *pvParameters){
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1){ vTaskDelay(10/portTICK_PERIOD_MS);}
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // Connect to Wi-Fi
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    
    // Init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();

    //disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

      struct tm timeinfo;
      if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return;
      }
    
    Serial.print(timeinfo.tm_mday);
    Serial.print("/");
    Serial.print(timeinfo.tm_mon + 1);
    Serial.print("/");
    Serial.print(timeinfo.tm_year + 1900);
    Serial.print(" ");
    Serial.print(timeinfo.tm_hour, DEC);
    Serial.print(":");
    Serial.print(timeinfo.tm_min, DEC);
    Serial.print(":");
    Serial.println(timeinfo.tm_sec, DEC);
    
    // When time needs to be re-set on a previously configured device, the
    // following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));

    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
  }
  while(1){
    Serial.println(pcTaskGetName(NULL));
    if(xSemaphoreTake(I2C_mutex, portTICK_PERIOD_MS) == pdTRUE){
      DateTime now = rtc.now();
      
      SensorData.Time_year = now.year();
      SensorData.Time_month = now.month();
      SensorData.Time_day = now.day();
      SensorData.Time_hour = now.hour();
      SensorData.Time_min = now.minute();
      SensorData.Time_sec = now.second();
      
      Serial.print(SensorData.Time_day);
      Serial.print('/');
      Serial.print(SensorData.Time_month);
      Serial.print('/');
      Serial.print(SensorData.Time_year);
      Serial.print(" (");
      Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
      Serial.print(") ");
      Serial.print(SensorData.Time_hour, DEC);
      Serial.print(':');
      Serial.print(SensorData.Time_min, DEC);
      Serial.print(':');
      Serial.print(SensorData.Time_sec, DEC);
      Serial.println();

      xSemaphoreGive(I2C_mutex);
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void SD_task(void *pvParameters){
  SD_start_check();
  writeFile(SD, "/hello.csv", "Hello,World,I,Am,PlanCare\n");
  while(1){
    Serial.println(pcTaskGetName(NULL));
    if(SD_start_check()){
      appendFile(SD, "/hello.csv", "Hello,World,I,Am,PlanCare2\n");
      vTaskDelay(4000/portTICK_PERIOD_MS);
    }

  }
}