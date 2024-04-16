#include <SoftwareSerial.h>
#include <Arduino.h>
#include "Wire.h"
#include "String.h"

#include<SHT25.h>

SHT25 H_Sens;

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

SemaphoreHandle_t I2C_mutex = NULL;

typedef struct Data_manager{
  int Time_year;
	int Time_month;
	int Time_day;
	int Time_hour;
	int Time_min;
	int Time_sec;

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

void DS3231_task(void *pvParameters); //Take time task

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

  // xTaskCreatePinnedToCore(DS3231_task, "DS3231_Task", 1024 * 4, NULL, 2, &RS485_task_handle, tskNO_AFFINITY);

  // xTaskCreatePinnedToCore(RS485_task, "RS485_Task", 1024 * 4, NULL, 3, &RS485_task_handle, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(SHT25_task, "SHT25_Task", 1024 * 4, NULL, 3, &SHT25_task_handle, tskNO_AFFINITY);
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
  Serial.println(pcTaskGetName(NULL));
  SHT25_begin();

  while(1){
    if (xSemaphoreTake(I2C_mutex, portTICK_PERIOD_MS) == pdTRUE)
    {
      SensorData.Env_temp = SHT25_getTemperature();
      SensorData.Env_Humi = SHT25_getHumidity();
      printEnvParameters(SensorData);

      xSemaphoreGive(I2C_mutex);
    }
    vTaskDelay(10000/portTICK_PERIOD_MS);
  }
}

void BH1750_task(void *pvParameters){

  while(1){

  }
}

void DS3231_task(void *pvParameters){

  while(1){

  }
}