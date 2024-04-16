#include <SoftwareSerial.h>

#include "Wire.h"
#include "String.h"

#include<SHT25.h>

SHT25 H_Sens;

// SHT25 I2C address is 0x40(64)
#define SHT25_Addr 0x40

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

  while(1){
    // if (xSemaphoreTake(I2C_mutex, portTICK_PERIOD_MS) == pdTRUE)
    // {
      Serial.print("Humidity    : ");
      Serial.print(H_Sens.getHumidity());
      Serial.println(" %RH");
      Serial.print("Temperature : ");
      Serial.print(H_Sens.getTemperature());
      Serial.println(" C");

    //   xSemaphoreGive(I2C_mutex);
    // }
    vTaskDelay(1000/portTICK_PERIOD_MS);
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