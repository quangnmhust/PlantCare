#include <PubSubClient.h>
#include <ThingSpeak.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <BH1750.h>
// #include <SHT25.h>
#include "Adafruit_SHT4x.h"
#include "nRF24L01.h"
#include "RF24.h"
#include <EasyButton.h>

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
#define Period_minute_time 5

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

#define BUT_GPIO 27
#define LED_GPIO 26
#define RF_CS 15
#define SD_CS 5

// const char* ssid = "minhquangng";
// const char* password =  "TBH123456";
String ssid = "minhquangng";
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

TaskHandle_t RS485_task_handle;
TaskHandle_t SHT25_task_handle;
TaskHandle_t BH1750_task_handle;
TaskHandle_t DS3231_task_handle;

TaskHandle_t MQTT_task_handle;
TaskHandle_t RF24_task_handle;

TaskHandle_t Display_task_handle;
TaskHandle_t Displaytime_task_handle;

TaskHandle_t Control_task_handle;
TaskHandle_t getData_task_handle;

static StaticSemaphore_t xSemaphoreBuffer;
SemaphoreHandle_t I2C_semaphore = NULL;
SemaphoreHandle_t MQTT_semaphore = NULL;

QueueHandle_t data_queue = NULL;

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

typedef struct Data_rf_manager{
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

} Data_rf;

// Data_rf SentRFData;

const uint64_t address = 0xF0F0F0F0E1LL; 

RF24 radio(4, 15, 18, 19, 23); //CE-CS-SCK-miso-mosi
BH1750 lightMeter;
RTC_DS3231 rtc;
// SHT25 H_Sens;
Adafruit_SHT4x H_Sens = Adafruit_SHT4x();
EasyButton button(BUT_GPIO);
LiquidCrystal_I2C lcd(0x27, 20, 4);

// WiFiClient  httpclient;
WiFiClient mqttclient;
PubSubClient client(mqttclient);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const byte request[] = {ADDRESS, FUNCTION_CODE, INITIAL_ADDRESS_HI, INITIAL_ADDRESS_LO, DATA_LENGTH_HI, DATA_LENGTH_LO, CHECK_CODE_LO, CHECK_CODE_HI};
byte sensorResponse[NUMBER_BYTES_RESPONES];

char SD_Frame_buff[126];
char SD_Frame[126];
char time_buff[64];
RTC_DATA_ATTR int errorCount = 0;
// String ssid, pss;


float convertBytesToFloat(byte hi, byte low){
  int intVal = (hi << 8) | low;
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
      Serial.print("File written: ");
        Serial.println(message);
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

  soilHumi = convertBytesToFloat(sensorResponse[3], sensorResponse[4]);
  soilTemp = convertBytesToFloat(sensorResponse[5], sensorResponse[6]);
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
    display.println((char)payload[i]);
    display.display();
  }
}

void buttonPressedFiveSeconds()
{
  Serial.println("Reseting the WiFi credentials");
  writeStringToFlash("", 0); // Reset the SSID
  writeStringToFlash("", 40); // Reset the Password
  Serial.println("Wifi credentials erased");
  Serial.println("Restarting the ESP");
  digitalWrite(LED_GPIO, HIGH);
  delay(500);
  digitalWrite(LED_GPIO, LOW);
  delay(500);
  digitalWrite(LED_GPIO, HIGH);
  delay(500);
  digitalWrite(LED_GPIO, LOW);
  digitalWrite(LED_GPIO, HIGH);
  delay(500);
  digitalWrite(LED_GPIO, LOW);
  delay(500);
  digitalWrite(LED_GPIO, HIGH);
  delay(500);
  digitalWrite(LED_GPIO, LOW);
  
  ESP.restart();
}

void buttonISR()
{
  // When button is being used through external interrupts, parameter INTERRUPT must be passed to read() function.
  button.read();
}
///////////////////////////////////////////////////////////////////////////////////////////

void DS3231_task(void *pvParameters); //Take time task
void Displaytime_task(void *pvParameters);
void MQTT_task(void *pvParameters);
void RF24_task(void *pvParameters);
void RS485_task(void *pvParameters); // Soil parameters
void SHT25_task(void *pvParameters); // Temp, humi parameters
void BH1750_task(void *pvParameters); // Lux parameter
void Control_task(void *pvParameters);
void getData_task(void *pvParameters);

void set_button(void){
  pinMode(SD_CS, OUTPUT);
  pinMode(RF_CS, OUTPUT);
  pinMode(LED_GPIO, OUTPUT);
  pinMode(BUT_GPIO, INPUT);
  digitalWrite(RF_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  digitalWrite(LED_GPIO, LOW);
  button.begin();
  button.onPressedFor(4000, buttonPressedFiveSeconds);
  if (button.supportsInterrupt())
  {
    button.enableInterrupt(buttonISR);
  }
}

void set_smartconfig(void){
  if (!EEPROM.begin(EEPROM_SIZE)) { //Init EEPROM
    Serial.println("failed to init EEPROM");
    delay(1000);
  } else {
    ssid = readStringFromFlash(0);
    Serial.print("SSID = ");
    Serial.println(ssid);
    pss = readStringFromFlash(40);
    Serial.print("psss = ");
    Serial.println(pss);
  }

}

bool esp_sd_start(void){
  if(!SD.begin()){
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Card Mount Failed");
    display.display();
    return 0;
  } else {
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("No SD card attached");
      display.display();
    }

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("SD Card Type:");
    if(cardType == CARD_MMC){
      display.setCursor(0,10);
      display.println("MMC");
      display.display();
    } else if(cardType == CARD_SD){
      display.setCursor(0,10);
      display.println("SDSC");
      display.display();
    } else if(cardType == CARD_SDHC){
        display.setCursor(0,10);
        display.println("SDHC");
        display.display();
    } else {
      display.setCursor(0,10);
      display.println("UNKNOWN");
      display.display();
    }
    // delay(1000);
    return 1;
  }
}

void esp_rtc_start(void){
  if (! rtc.begin()) {

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Couldn't find RTC");
    lcd.setCursor(0, 2);
    lcd.print("Couldn't find RTC");
    display.display();
  } else {
   if (rtc.lostPower()) {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("RTC lost power");
      lcd.setCursor(0, 2);
      lcd.print("RTC lost power");
      display.display();
      WiFi.begin(ssid.c_str(), pss.c_str());
      
      // Init and get the time
      while (WiFi.status() != WL_CONNECTED) {
        digitalWrite (LED_GPIO, HIGH);
        lcd.clear();
        display.setCursor(0,10);
        display.fillRect(0, 10, SCREEN_WIDTH, 9, BLACK);
        display.println("Connecting to WiFi");
        lcd.setCursor(0, 0);
        lcd.print("Connecting to WiFi");
        display.display();
        display.setCursor(0,30);
        display.println("SSID:"+String(ssid));
        display.setCursor(0,40);
        display.println("PASS:"+String(pss));
        display.display();
        delay(125);
        digitalWrite (LED_GPIO, LOW);
        display.setCursor(0,10);
        display.fillRect(0, 10, SCREEN_WIDTH, 9, BLACK);
        lcd.setCursor(0, 0);
        lcd.print("Connecting to WiFi.   ");
        lcd.setCursor(0,20);
        display.println(String(ssid));
        lcd.setCursor(0,30);
        display.println(String(pss));
        display.println("Connecting to WiFi.");
        display.display();
        delay(125);
        digitalWrite (LED_GPIO, HIGH);
        display.setCursor(0,10);
        display.fillRect(0, 10, SCREEN_WIDTH, 9, BLACK);
        lcd.setCursor(0, 0);
        lcd.print("Connecting to WiFi..  ");
        display.println("Connecting to WiFi..");
        display.display();
        delay(125);
        digitalWrite (LED_GPIO, LOW);
        display.setCursor(0,10);
        display.fillRect(0, 10, SCREEN_WIDTH, 9, BLACK);
        lcd.setCursor(0, 0);
        lcd.print("Connecting to WiFi...   ");
        display.println("Connecting to WiFi...");
        display.display();
        delay(125);
      }
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

      struct tm timeinfo;
      if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        rtc.adjust(DateTime(0, 0, 0, 0, 0, 0));
      }
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    } else {
      DateTime time_start = rtc.now();
      display.clearDisplay();
      lcd.clear();
      display.setCursor(0,0);
      lcd.setCursor(0, 0);
      lcd.print("RTC ready!");
      display.println("RTC ready!");
      display.setCursor(0,20);
      display.println("Time:");
      lcd.setCursor(0, 1);
      lcd.print("Time:");
      display.setCursor(0,30);
      display.println(String(time_start.timestamp(DateTime::TIMESTAMP_FULL)));
      lcd.setCursor(0, 2);
      lcd.print(String(time_start.timestamp(DateTime::TIMESTAMP_FULL)));
      display.display();
    }
  }
}

void esp_connect_mqtt(void){
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Connecting to MQTT...");
    display.display();

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
      delay(2000);
    }
  }
  // client.subscribe("modelParam");
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

void esp_spi_start(void){
  digitalWrite(SD_CS, LOW);
  if(!esp_sd_start()){
  } else {
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    display.setCursor(0,20);
    lcd.setCursor(0, 0);
    lcd.print("SD Size: " + String(cardSize) + String(" MB"));
    display.println("SD Card Size:");
    display.setCursor(0,30);
    display.println(cardSize + String(" MB"));
    display.display();
  }
  digitalWrite(SD_CS, HIGH);

  delay(500);
  
  digitalWrite(RF_CS, LOW); 
  if(!radio.begin()){
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("RF Failed!");
    lcd.setCursor(0, 1);
    lcd.print("RF Failed!");
    display.display();
  }else{
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("RF Ready!");
    lcd.setCursor(0, 1);
    lcd.print("RF Ready!");
    display.display();
    radio.setDataRate(RF24_250KBPS);
    radio.openWritingPipe(address); //Setting the address where we will send the data
    radio.setPALevel(RF24_PA_MIN);  //You can set it as minimum or maximum depending on the distance between the transmitter and receiver.
    radio.stopListening();
  }

  // radio.setChannel(2);
  // radio.setPayloadSize(24);
  // radio.setPALevel(RF24_PA_MIN);
  // radio.setDataRate(RF24_250KBPS);
  // radio.openWritingPipe(address);
  // radio.stopListening();
  digitalWrite(RF_CS, HIGH);

  // display.setCursor(0,20);
  // display.println("SetChannel: 2");
  // display.setCursor(0,30);
  // display.println("SetDataRate: " + String(RF24_250KBPS));
  // display.setCursor(0,40);
  // display.println("SetAddress: " + String(address));
  // display.display();
}

void esp_start(void){
  Serial.begin(9600);
  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
  }
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay();

  lcd.init(); // initialize the lcd
  lcd.backlight();

  set_button();
  // esp_connect_wifi();
  // esp_connect_mqtt();
  esp_spi_start();
  delay(1500);
  esp_rtc_start();
  delay(1500);
}

void setup() {
  esp_start();

  I2C_semaphore = xSemaphoreCreateCountingStatic( 4, 4, &xSemaphoreBuffer );
  if (I2C_semaphore != NULL)
  {
    display.clearDisplay();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("I2C_semaphore created");
    display.setCursor(0,0);
    display.println("I2C_semaphore created");
    display.display();
  }
  MQTT_semaphore = xSemaphoreCreateMutex();
  if (MQTT_semaphore != NULL)
  {
    display.setCursor(0,20);
    display.println("MQTT_semaph created");
    lcd.setCursor(0, 1);
    lcd.print("MQTT_semaph created");
    display.display();
  }

  data_queue = xQueueCreate(20, sizeof(Data_t));
  if (data_queue != NULL)
  {
    display.setCursor(0,40);
    display.println("Data_queue created");
    lcd.setCursor(0, 2);
    lcd.print("Data_queue created");
    display.display();
    delay(1000);
    display.clearDisplay();
    lcd.clear();
  }

  H_Sens.begin();
  H_Sens.setPrecision(SHT4X_HIGH_PRECISION);
  switch (H_Sens.getPrecision()) {
     case SHT4X_HIGH_PRECISION: 
       Serial.println("High precision");
       break;
     case SHT4X_MED_PRECISION: 
       Serial.println("Med precision");
       break;
     case SHT4X_LOW_PRECISION: 
       Serial.println("Low precision");
       break;
  }
  H_Sens.setHeater(SHT4X_NO_HEATER);
  // lightMeter.begin();
  writeFile(SD, "/data.csv", "Date,Month,Year,Hour,Min,Sec,Lux,E_Tem,E_Hum,S_Tem,S_Hum,S_pH,S_Ni,S_Ph,S_Ka\n");
  // ThingSpeak.begin(mqttclient);

  xTaskCreatePinnedToCore(Control_task, "Control_task", 1024 * 4, NULL, 2, &Control_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(DS3231_task, "DS3231_Task", 1024 * 4, NULL, 3, &DS3231_task_handle, tskNO_AFFINITY);
}
 
void loop() {

  button.update();
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
  sensors_event_t humidity, temp;
  while(1){
    if (xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      H_Sens.getEvent(&humidity, &temp);
      SensorData.Env_temp = temp.temperature;
      SensorData.Env_Humi = humidity.relative_humidity;
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
    vTaskDelete(NULL);
  }
}

void Displaytime_task(void *pvParameters){
  Data_t time_data = {};
  while(1){

    time_data.Time_year = SensorData.Time_year;
    time_data.Time_month = SensorData.Time_month;
    time_data.Time_day = SensorData.Time_day;
    time_data.Time_hour = SensorData.Time_hour;
    time_data.Time_min = SensorData.Time_min;
    time_data.Time_sec = SensorData.Time_sec;

    memset(time_buff, 0, 64);
    sprintf(time_buff, "%02d/%02d/%02d %02d:%02d:%02d", 
    time_data.Time_day,
    time_data.Time_month,
    time_data.Time_year,
    time_data.Time_hour,
    time_data.Time_min,
    time_data.Time_sec);


    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      // display.fillRect(0, 0, SCREEN_WIDTH, 9, BLACK);
      // display.setCursor(0,0);
      // display.print(String(time_buff));
      lcd.setCursor(0, 0);
      lcd.print(String(time_buff));
      // display.display();
      xSemaphoreGive(I2C_semaphore);
    }
    vTaskDelete(NULL);
  }
}

void DS3231_task(void *pvParameters){
  Serial.println(pcTaskGetName(NULL));
  while(1){
    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      DateTime now = rtc.now();
      
      SensorData.Time_year = now.year();
      SensorData.Time_month = now.month();
      SensorData.Time_day = now.day();
      SensorData.Time_hour = now.hour();
      SensorData.Time_min = now.minute();
      SensorData.Time_sec = now.second();
      xSemaphoreGive(I2C_semaphore);
    }
    xTaskCreatePinnedToCore(Displaytime_task, "Displaytime_task", 1024 * 4, NULL, 4, &Displaytime_task_handle, tskNO_AFFINITY);
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void Display_task(void *pvParameters){
  Serial.println(pcTaskGetName(NULL));
  while(1){
    if(xSemaphoreTake(I2C_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      // display.clearDisplay();
      int count_d=1;
      while(count_d<3){
        display.fillRect(0, 8, SCREEN_WIDTH, 60, BLACK);
        display.setCursor(0,11);
        display.println("Env-Lux: " + String(SensorData.Env_Lux));
        display.setCursor(0,22);
        display.println("Env-T/H: " + String(SensorData.Env_temp)+"/"+String(SensorData.Env_Humi));
        lcd.setCursor(0, 1);
        lcd.print("Env-T/H: ");
        lcd.setCursor(8, 1);
        lcd.print(SensorData.Env_temp);
        lcd.setCursor(13, 1);
        lcd.print("/");
        lcd.setCursor(14, 1);
        lcd.print(SensorData.Env_Humi);
        display.setCursor(0,33);
        display.println("Soi-T/H: " + String(SensorData.Soil_temp)+"/"+String(SensorData.Soil_humi));
        display.setCursor(0,44);
        display.println("Soi-pH: " + String(SensorData.Soil_pH));
        lcd.setCursor(0, 3);
        display.setCursor(0,55);
        display.println("NPK:" + String(SensorData.Soil_Nito)+"/"+String(SensorData.Soil_Phosp)+"/"+String(SensorData.Soil_Kali));
        lcd.print("NPK:" + String(SensorData.Soil_Nito)+"/"+String(SensorData.Soil_Phosp)+"/"+String(SensorData.Soil_Kali));
        display.display();
        count_d++;
      }
      xSemaphoreGive(I2C_semaphore);
    }
    vTaskDelete(NULL);
  }
}

void MQTT_task(void *pvParameters){
  Data_t mqtt_data = {};
  while(1){
    if(uxQueueMessagesWaiting(data_queue) != 0){
      if (xQueueReceive(data_queue, &mqtt_data, portMAX_DELAY) == pdPASS) {
        Serial.print("MQTT data waiting to read ");
        Serial.print(uxQueueMessagesWaiting(data_queue));
        Serial.print(", Available space ");
        Serial.println(uxQueueSpacesAvailable(data_queue));
        WORD_ALIGNED_ATTR char mqttMessage[512];
        sprintf(mqttMessage, "{\n\t\"Time_real_Date\":\"%02d/%02d/%02d %02d:%02d:%02d\",\n\t\"Soil_temp\":%.2f,\n\t\"Soil_humi\":%.2f,\n\t\"Soil_pH\":%.2f,\n\t\"Soil_Nito\":%.2f,\n\t\"Soil_Kali\":%.2f,\n\t\"Soil_Phosp\":%.2f,\n\t\"Env_temp\":%.2f,\n\t\"Env_Humi\":%.2f,\n\t\"Env_Lux\":%.2f\n}",
                mqtt_data.Time_day,
                mqtt_data.Time_month,
                mqtt_data.Time_year,
                mqtt_data.Time_hour,
                mqtt_data.Time_min,
                mqtt_data.Time_sec,
                mqtt_data.Soil_temp,
                mqtt_data.Soil_humi,
                mqtt_data.Soil_pH,
                mqtt_data.Soil_Nito,
                mqtt_data.Soil_Phosp,
                mqtt_data.Soil_Kali,
                mqtt_data.Env_temp,
                mqtt_data.Env_Humi,
                mqtt_data.Env_Lux
              );
        Serial.println(mqttMessage);
        // if (client.connect("ESP32Client", mqttUser, mqttPassword)) {
          if(client.publish("modelParam", mqttMessage)){
            digitalWrite (LED_GPIO, HIGH);
            vTaskDelay(2000/portTICK_PERIOD_MS);
            digitalWrite (LED_GPIO, LOW);
          }
        // }
      }
    }
    vTaskDelete(NULL);
  }
}

void RF24_task(void *pvParameters){
  Serial.println(pcTaskGetName(NULL));
  // Data_t mqtt_data={};
  Data_t mqtt_data={};
  Data_rf rf_data={};

  while(1){
    if(uxQueueMessagesWaiting(data_queue) != 0){
      if (xQueueReceive(data_queue, &mqtt_data, portMAX_DELAY) == pdPASS) {
        digitalWrite(LED_GPIO, HIGH);
        Serial.print("MQTT data waiting to read ");
        Serial.print(uxQueueMessagesWaiting(data_queue));
        Serial.print(", Available space ");
        Serial.println(uxQueueSpacesAvailable(data_queue));

        if(xSemaphoreTake(MQTT_semaphore, portTICK_PERIOD_MS) == pdTRUE){
          digitalWrite(RF_CS, LOW);
          vTaskDelay(10/portTICK_PERIOD_MS);
          Serial.println("send 1...");
          rf_data = {
            1,
            mqtt_data.Time_year,
            mqtt_data.Time_month,
            mqtt_data.Time_day,
            mqtt_data.Time_hour,
            mqtt_data.Time_min,
            mqtt_data.Time_sec, 
            mqtt_data.Soil_temp,
            mqtt_data.Soil_humi, 
            mqtt_data.Soil_pH, 
            mqtt_data.Soil_Nito,
            mqtt_data.Soil_Phosp,
            mqtt_data.Soil_Kali
          };

          radio.write(&rf_data, sizeof(Data_rf));
          vTaskDelay(50/portTICK_PERIOD_MS);
          Serial.println("send 2...");
          rf_data = {
            2,
            0,
            0,
            0,
            0,
            0,
            0, 
            mqtt_data.Env_temp,
            mqtt_data.Env_Humi,
            mqtt_data.Env_Lux, 
            0,
            0,
            0
          };

          radio.write(&rf_data, sizeof(Data_rf));
          digitalWrite(RF_CS, HIGH);
          xSemaphoreGive(MQTT_semaphore);
        }
      }
    }
    vTaskDelete(NULL);
  }
}

void getData_task(void *pvParameters){
  Serial.println(pcTaskGetName(NULL));
  vTaskDelay(200/portTICK_PERIOD_MS);
  Data_t temp_data = {};
  while(1){
    temp_data.Env_Lux = SensorData.Env_Lux;
    temp_data.Env_Humi = SensorData.Env_Humi;
    temp_data.Env_temp = SensorData.Env_temp;
    temp_data.Soil_temp = SensorData.Soil_temp;
    temp_data.Soil_humi = SensorData.Soil_humi;
    temp_data.Soil_pH = SensorData.Soil_pH;
    temp_data.Soil_Nito = SensorData.Soil_Nito;
    temp_data.Soil_Phosp = SensorData.Soil_Phosp;
    temp_data.Soil_Kali = SensorData.Soil_Kali;
    temp_data.Time_year = SensorData.Time_year;
    temp_data.Time_month = SensorData.Time_month;
    temp_data.Time_day = SensorData.Time_day;
    temp_data.Time_hour = SensorData.Time_hour;
    temp_data.Time_min = SensorData.Time_min;
    temp_data.Time_sec = SensorData.Time_sec;

    if (xQueueSendToBack(data_queue, (void *)&temp_data, 1000/portMAX_DELAY) == pdTRUE  ){
			Serial.print("MQTT data waiting to read ");
      Serial.print(uxQueueMessagesWaiting(data_queue));
      Serial.print(", Available space ");
      Serial.println(uxQueueSpacesAvailable(data_queue));
      // xTaskCreatePinnedToCore(MQTT_task, "MQTT_task", 1024 * 4, NULL, 7, &MQTT_task_handle, tskNO_AFFINITY);
      xTaskCreatePinnedToCore(RF24_task, "RF24_task", 1024 * 4, NULL, 7, &RF24_task_handle, tskNO_AFFINITY);
		}

    memset(SD_Frame_buff, 0, 126);
    sprintf(SD_Frame_buff, "%02d,%02d,%02d,%02d,%02d,%02d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
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

    Serial.println(SD_Frame_buff);
    xTaskCreatePinnedToCore(Display_task, "Display_task", 1024 * 4, NULL, 8, &Display_task_handle, tskNO_AFFINITY);

    if(xSemaphoreTake(MQTT_semaphore, portTICK_PERIOD_MS) == pdTRUE){
      digitalWrite(SD_CS, LOW);
      vTaskDelay(10/portTICK_PERIOD_MS);
      appendFile(SD, "/data.csv", SD_Frame_buff);
      digitalWrite(SD_CS, HIGH);
      xSemaphoreGive(MQTT_semaphore);
    }
    vTaskDelay(2000/portTICK_PERIOD_MS);
    digitalWrite(LED_GPIO, LOW);
    vTaskDelete(NULL);
  }
}

void Control_task(void *pvParameters){
  int count= 0;
  while(1){
    vTaskDelay(100/portTICK_PERIOD_MS);
  // xTaskCreatePinnedToCore(RS485_task, "RS485_Task", 1024 * 4, NULL, 5, &RS485_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(SHT25_task, "SHT25_Task", 1024 * 4, NULL, 5, &SHT25_task_handle, tskNO_AFFINITY);
  // xTaskCreatePinnedToCore(BH1750_task, "BH1750_Task", 1024 * 4, NULL, 5, &BH1750_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(getData_task, "getData_task", 1024 * 4, NULL, 6, &getData_task_handle, tskNO_AFFINITY);
  // xTaskCreatePinnedToCore(RF24_task, "RF24_task", 1024 * 4, NULL, 7, &RF24_task_handle, tskNO_AFFINITY);
  count++;
  vTaskDelay(Period_minute_time*60000-100/portTICK_PERIOD_MS);
  Serial.println(count);
  if(count == 25){
    ESP.restart();
  }
  }
}
