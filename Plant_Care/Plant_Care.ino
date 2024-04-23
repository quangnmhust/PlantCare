#include <ThingSpeak.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
#include "EEPROM.h"

BH1750 lightMeter;
RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

char SD_Frame[1024];

#define uS_TO_M_FACTOR 1000000
#define TIME_TO_SLEEP 10
#define Period_minute_time 1

int LED_BUILTIN = 2;

//set length of char string
#define LENGTH(x) (strlen(x) + 1)
#define EEPROM_SIZE 200
//--------------------------------------------------
//WiFi credential reset pin (Boot button on ESP32)
#define WiFi_rst 0
unsigned long rst_millis;
//--------------------------------------------------
//variable to store ssid and password
String ssid;
String pss;
//--------------------------------------------------

// kênh số mấy, Key để ghi(Write Key)
const int myChannelNumber = 2515584;
const char * myWriteAPIKey = "8TGOAVM92D494KS1";

WiFiClient  client;
// const char* ssid     = "minhquangng";
// const char* password = "TBH123456";
// const char* ssid     = "Sanslab";
// const char* password = "sanslab@";

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


//Rain sensor
#define RAIN_ADDRESS 0x01
#define RAIN_FUNCTION_CODE 0x03
#define RAIN_INITIAL_ADDRESS_HI 0x00
#define RAIN_INITIAL_ADDRESS_LO 0x00
#define RAIN_DATA_LENGTH_HI 0x00
#define RAIN_DATA_LENGTH_LO 0x01
#define RAIN_CHECK_CODE_LO 0x84
#define RAIN_CHECK_CODE_HI 0x0A
#define RAIN_NUMBER_BYTES_RESPONES 7
// Request frame for the soil sensor
const byte rainrequest[] = {RAIN_ADDRESS, RAIN_FUNCTION_CODE, RAIN_INITIAL_ADDRESS_HI, RAIN_INITIAL_ADDRESS_LO, RAIN_DATA_LENGTH_HI, RAIN_DATA_LENGTH_LO, RAIN_CHECK_CODE_LO, RAIN_CHECK_CODE_HI};
byte rainsensorResponse[RAIN_NUMBER_BYTES_RESPONES];
SoftwareSerial mod(25,26); // Software serial for RS485 communication

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

TaskHandle_t RS485_task_handle;
TaskHandle_t Rain_task_handle;
TaskHandle_t SHT25_task_handle;
TaskHandle_t BH1750_task_handle;
TaskHandle_t DS3231_task_handle;
TaskHandle_t MQTT_task_handle;
TaskHandle_t Display_task_handle;


static StaticSemaphore_t xSemaphoreBuffer;
SemaphoreHandle_t I2C_semaphore = NULL;

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
////////////////////////////////////////////////////////////////////


//SHT25 Function
/////////////////////////////////////////////////////////////////////

float TEMP, RH, S_T, S_RH;
byte T_Delay = 85;  //for 14 bit resolution
byte RH_Delay = 29; //for 12 bit resolution 

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
        Serial.print(message);
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


void DS3231_task(void *pvParameters); //Take time task
void MQTT_task(void *pvParameters); //MQTT Task

void RS485_task(void *pvParameters); // Soil parameters
void Rain_task(void *pvParameters); // Rain parameters
void SHT25_task(void *pvParameters); // Temp, humi parameters
void BH1750_task(void *pvParameters); // Lux parameter

void setup() {
  Serial.begin(9600); //init serial with computer
  Wire.begin();

  I2C_semaphore = xSemaphoreCreateCountingStatic( 4, 4, &xSemaphoreBuffer );  //Init I2C_semaphore
  if (I2C_semaphore != NULL)
  {
    Serial.print("I2C_semaphore created: ");
    Serial.println(uxSemaphoreGetCount(I2C_semaphore));
  }

  // if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
  //   Serial.println(F("SSD1306 allocation failed"));
  // }
  // delay(2000);
  // display.clearDisplay();
  // display.setTextSize(1);
  // display.setTextColor(WHITE);

  pinMode (LED_BUILTIN, OUTPUT);
  pinMode(WiFi_rst, INPUT);
  if (!EEPROM.begin(EEPROM_SIZE)) { //Init EEPROM
    Serial.println("failed to init EEPROM");
    delay(1000);
  }
  //------------------------------------------------
  else
  {
    //Read SSID stored at address 0
    ssid = readStringFromFlash(0);
    Serial.print("SSID = ");
    Serial.println(ssid);
    // Read Password stored at address 40
    pss = readStringFromFlash(40);
    Serial.print("psss = ");
    Serial.println(pss);
  }
  //------------------------------------------------
  WiFi.begin(ssid.c_str(), pss.c_str());
  delay(3000);
  //------------------------------------------------
  //if WiFi is not connected
  if (WiFi.status() != WL_CONNECTED)
  {
    //Init WiFi as Station, start SmartConfig
    WiFi.mode(WIFI_AP_STA);
    WiFi.beginSmartConfig();
    //----------------------------------------------
    //Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    // display.setCursor(0,0);
    // // Display static text
    // display.println("Waiting for SmartConfig.");
    // display.display();
    while (!WiFi.smartConfigDone()) {
      delay(500);
      Serial.print(".");
    }
    //----------------------------------------------
    Serial.println("");
    Serial.println("SmartConfig received.");
    //----------------------------------------------
    //Wait for WiFi to connect to AP
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      digitalWrite(LED_BUILTIN, HIGH);
      delay(125);
      digitalWrite(LED_BUILTIN, LOW);
      delay(125);
    }
    //----------------------------------------------
    Serial.println("WiFi Connected.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    //----------------------------------------------
    //read the connected WiFi SSID and password
    ssid = WiFi.SSID();
    pss = WiFi.psk();
    //----------------------------------------------
    Serial.print("SSID:");
    Serial.println(ssid);
    Serial.print("PASS:");
    Serial.println(pss);
    Serial.println("Storing SSID & PASSWORD in EEPROM");
    //----------------------------------------------
    //store the ssid at address 0
    writeStringToFlash(ssid.c_str(), 0);
    //store the password at address 40
    writeStringToFlash(pss.c_str(), 40);
    //----------------------------------------------
    Serial.println("OK");
  } else {Serial.println("WiFi Connected");}
  //------------------------------------------------

  SHT25_begin();
  lightMeter.begin();

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1){ vTaskDelay(10/portTICK_PERIOD_MS);}
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // Connect to Wi-Fi
    if (!EEPROM.begin(EEPROM_SIZE)) { //Init EEPROM
    Serial.println("failed to init EEPROM");
    delay(1000);
    }
    //------------------------------------------------
    else
    {
      //Read SSID stored at address 0
      ssid = readStringFromFlash(0);
      Serial.print("SSID = ");
      Serial.println(ssid);
      // Read Password stored at address 40
      pss = readStringFromFlash(40);
      Serial.print("psss = ");
      Serial.println(pss);
    }
    //------------------------------------------------
    WiFi.begin(ssid.c_str(), pss.c_str());
    delay(3000);
    
    // Init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();

    // //disconnect WiFi as it's no longer needed
    // WiFi.disconnect(true);
    // WiFi.mode(WIFI_OFF);

    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
    }

    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
  }

  SD_start_check();
  writeFile(SD, "/data.csv", "Date,Month,Year,Hour,Min,Sec,Lux,E_Tem,E_Hum,S_Tem,S_Hum,S_pH,S_Ni,S_Ph,S_Ka\n");

  // // Connect to Wifi
  // Serial.println();
  // Serial.print("Connecting to ");
  // Serial.println(ssid);
  // WiFi.begin(ssid, password);

  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println("WiFi connected");
  // Serial.println("IP address: ");
  // Serial.println(WiFi.localIP());
  // WiFi.mode(WIFI_STA);

  ThingSpeak.begin(client);

  xTaskCreatePinnedToCore(DS3231_task, "DS3231_Task", 1024 * 4, NULL, 2, &DS3231_task_handle, tskNO_AFFINITY);
  // xTaskCreatePinnedToCore(MQTT_task, "MQTT_Task", 1024 * 4, NULL, 5, &MQTT_task_handle, tskNO_AFFINITY);

  xTaskCreatePinnedToCore(RS485_task, "RS485_Task", 1024 * 4, NULL, 3, &RS485_task_handle, tskNO_AFFINITY);
  // xTaskCreatePinnedToCore(Rain_task, "Rain_Task", 1024 * 4, NULL, 3, &Rain_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(SHT25_task, "SHT25_Task", 1024 * 4, NULL, 3, &SHT25_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(BH1750_task, "BH1750_Task", 1024 * 4, NULL, 3, &BH1750_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(Display_task, "Display_task", 1024 * 4, NULL, 3, &Display_task_handle, tskNO_AFFINITY);
}

void loop() {
  // No thing in here because everything run in tasks
  rst_millis = millis();
  while (digitalRead(WiFi_rst) == LOW) {
    // Wait till boot button is pressed 
  }
  //----------------------------------------------
  // check the button press time if it is greater 
  //than 3sec clear wifi cred and restart ESP 
  if (millis() - rst_millis >= 3000) {
    int reset_count = 0;
    while(reset_count > 2){
      digitalWrite(LED_BUILTIN, HIGH);
      delay(125);
      digitalWrite(LED_BUILTIN, LOW);
      delay(125);
      reset_count++;
    }
    Serial.println("Reseting the WiFi credentials");
    writeStringToFlash("", 0); // Reset the SSID
    writeStringToFlash("", 40); // Reset the Password
    Serial.println("Wifi credentials erased");
    Serial.println("Restarting the ESP");
    delay(500);
    ESP.restart();
  }
}

//Task

void RS485_task(void *pvParameters){
  Serial2.begin(4800);

  while(1){
    // Serial.println(pcTaskGetName(NULL));
    Serial2.write(request, sizeof(request));
    vTaskDelay(10/portTICK_RATE_MS);
    // Wait for the response from the sensor or timeout after 1 second
    unsigned long startTime = millis();
    while (Serial2.available() < NUMBER_BYTES_RESPONES && millis() - startTime < 1000)
    {
      vTaskDelay(1/portTICK_RATE_MS);
    }
    // Serial.println(Serial2.available());

    if (Serial2.available() >= NUMBER_BYTES_RESPONES){
      // Read the response from the sensor
      byte index = 0;
      while (Serial2.available() && index < NUMBER_BYTES_RESPONES){
        sensorResponse[index] = Serial2.read();
        // Serial.print(sensorResponse[index], HEX);
        // Serial.print(" ");
        index++;
      }
      // Serial.println(" $End of Rx data");
      getSoilSensorParameter();
    } else {
      Serial.println("Sensor timeout or incomplete frame");
    }
    vTaskDelay(Period_minute_time*60000/portTICK_PERIOD_MS);
  }
}

void Rain_task(void *pvParameters){
  mod.begin(4800);

  while(1){
    // Serial.println(pcTaskGetName(NULL));
    // Send the request frame to the soil sensor
    mod.write(rainrequest, sizeof(rainrequest));
    delay(10);
  
    // Wait for the response from the sensor or timeout after 1 second
    unsigned long startTime = millis();
    while (mod.available() < 9 && millis() - startTime < 1000)
    {
      delay(1);
    }
  
    if (mod.available() >= RAIN_NUMBER_BYTES_RESPONES) // If valid response received
    {
      // Read the response from the sensor
      byte index = 0;
      while (mod.available() && index < RAIN_NUMBER_BYTES_RESPONES)
      {
        rainsensorResponse[index] = mod.read();
        Serial.print(rainsensorResponse[index], HEX); // Print the received byte in HEX format
        Serial.print(" ");
        index++;
      }
      Serial.println(" $End of Rx data");
  
      // Parse and calculate the Rainfall value
      int Sensor_Int = int(rainsensorResponse[3] << 8 | rainsensorResponse[4]);
      float Senor_Float = Sensor_Int / 10.0;
  
      Serial.print("Float result: ");
      Serial.println(Senor_Float);
    }
    else
    {
      // Print error message if no valid response received
      Serial.println("Sensor timeout or incomplete frame");
    }
    // delay(1000);
    vTaskDelay(Period_minute_time*60000/portTICK_PERIOD_MS);
  }
}

void SHT25_task(void *pvParameters){
  while(1){
    if (xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      // Serial.println(pcTaskGetName(NULL));
      SensorData.Env_temp = SHT25_getTemperature();
      SensorData.Env_Humi = SHT25_getHumidity();
      xSemaphoreGive(I2C_semaphore);
    }
    vTaskDelay(Period_minute_time*60000/portTICK_PERIOD_MS);
  }
}

void BH1750_task(void *pvParameters){
  while(1){
    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      // Serial.println(pcTaskGetName(NULL));
      SensorData.Env_Lux = lightMeter.readLightLevel();
      xSemaphoreGive(I2C_semaphore);
    }
    vTaskDelay(Period_minute_time*60000/portTICK_PERIOD_MS);
  }
}

void DS3231_task(void *pvParameters){
  while(1){
    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      // Serial.println(pcTaskGetName(NULL));
      DateTime now = rtc.now();
      
      SensorData.Time_year = now.year();
      SensorData.Time_month = now.month();
      SensorData.Time_day = now.day();
      SensorData.Time_hour = now.hour();
      SensorData.Time_min = now.minute();
      SensorData.Time_sec = now.second();
      xSemaphoreGive(I2C_semaphore);
    }
    // vTaskDelay(Period_minute_time*60000/portTICK_PERIOD_MS);
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void Display_task(void *pvParameters){
  while(1){
    vTaskDelay(1000/portTICK_PERIOD_MS);
    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      // Serial.println(pcTaskGetName(NULL));

      memset(SD_Frame, 0, 1024);
      // "Date,Month,Year,Hour,Min,Sec,Lux,E_Tem,E_Hum,S_Tem,S_Hum,S_pH,S_Ni,S_Ph,S_Ka\n"
      sprintf(SD_Frame, "%02d,%02d,%02d,%02d,%02d,%02d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", 
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

      Serial.print(SensorData.Time_day);
      Serial.print('/');
      Serial.print(SensorData.Time_month);
      Serial.print('/');
      Serial.print(SensorData.Time_year);
      Serial.print(" ");
      Serial.print(SensorData.Time_hour, DEC);
      Serial.print(':');
      Serial.print(SensorData.Time_min, DEC);
      Serial.print(':');
      Serial.println(SensorData.Time_sec, DEC);
      Serial.print("Lux env: ");
      Serial.print(SensorData.Env_Lux);
      Serial.println(" lux");
      Serial.print("Temp env: ");
      Serial.print(SensorData.Env_temp);
      Serial.println(" oC");
      Serial.print("Humi env: ");
      Serial.print(SensorData.Env_Humi);
      Serial.println(" %");
      Serial.print("Temp soil: ");
      Serial.print(SensorData.Soil_temp);
      Serial.println(" oC");
      Serial.print("Humi soil: ");
      Serial.print(SensorData.Soil_humi);
      Serial.println(" %");
      Serial.print("pH soil: ");
      Serial.print(SensorData.Soil_pH);
      Serial.println(" pH");
      Serial.print("SD_Frame: ");
      Serial.print(SD_Frame);

      if(!SensorData.Time_day == 0){
        appendFile(SD, "/data.csv", SD_Frame);
        // check_done_data = true;
      }

        // set the fields with the values
      ThingSpeak.setField(1, SensorData.Env_Lux);
      ThingSpeak.setField(2, SensorData.Env_temp);
      ThingSpeak.setField(3, SensorData.Env_Humi);
      ThingSpeak.setField(4, SensorData.Soil_humi);
      ThingSpeak.setField(5, SensorData.Soil_pH);
      ThingSpeak.setField(6, SensorData.Soil_Nito);
      ThingSpeak.setField(7, SensorData.Soil_Phosp);
      ThingSpeak.setField(8, SensorData.Soil_Kali);

      // write to the ThingSpeak channel
      int x1 = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
      if(x1 == 200){
        Serial.println("Channel update successful.");
      }
      else{
        Serial.println("Problem updating channel. HTTP error code " + String(x1));
      }

      Serial.println("-----------------------------------------------------------");
      xSemaphoreGive(I2C_semaphore);
    }
    vTaskDelay(Period_minute_time*60000-1000/portTICK_PERIOD_MS);
    // vTaskDelay(5000/portTICK_PERIOD_MS);
  }
}