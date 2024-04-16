#include <SoftwareSerial.h>

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




float convertBytesToFloat(byte hi, byte low){
  int intVal = (hi << 8) | low;
  //Serial.println(intVal);
  return (float)intVal/10.0;
}


float soilHumi;
float soilTemp;
float soilConduct;
float soilPH;
float soilN;
float soilP;
float soilK;


void getSoilSensorParameter(){
    soilHumi = convertBytesToFloat(sensorResponse[3], sensorResponse[4]);
    soilTemp = convertBytesToFloat(sensorResponse[5], sensorResponse[6]);
    soilConduct = convertBytesToFloat(sensorResponse[7], sensorResponse[8]);
    soilPH = convertBytesToFloat(sensorResponse[9], sensorResponse[10]);
    soilN = convertBytesToFloat(sensorResponse[11], sensorResponse[12]);
    soilP = convertBytesToFloat(sensorResponse[13], sensorResponse[14]);
    soilK = convertBytesToFloat(sensorResponse[15], sensorResponse[16]);
}


void printSoilParameters(){
    Serial.print("Độ ẩm: ");
    Serial.print(soilHumi);
    Serial.println(" %RH");


    Serial.print("Nhiệt độ: ");
    Serial.print(soilTemp);
    Serial.println(" oC");


    Serial.print("Conductivity: ");
    Serial.print(soilConduct);
    Serial.println(" uS/cm");


    Serial.print("pH hiện tại: ");
    Serial.print(soilPH);
    Serial.println(" ");


    Serial.print("Nito hiện tại: ");
    Serial.print(soilN);
    Serial.println(" ppm");


    Serial.print("Photpho hiện tại: ");
    Serial.print(soilP);
    Serial.println(" ppm");


     Serial.print("Kali hiện tại: ");
    Serial.print(soilK);
    Serial.println(" ppm");
}


SoftwareSerial mod(2, 3); // Software serial for RS485 communication
 
void setup()
{
  Serial.begin(9600); // Initialize serial communication for debugging
  mod.begin(4800);    // Initialize software serial communication at 4800 baud rate




  //Serial3.begin(4800);
  delay(500);
}
 
 
void loop()
{
 
  // Send the request frame to the soil sensor
  mod.write(request, sizeof(request));
  delay(10);
 
  // Wait for the response from the sensor or timeout after 1 second
  unsigned long startTime = millis();
  while (mod.available() < NUMBER_BYTES_RESPONES && millis() - startTime < 1000)
  {
    delay(1);
  }
Serial.println(mod.available());


 
  if (mod.available() >= NUMBER_BYTES_RESPONES) // If valid response received
  {
    // Read the response from the sensor
    byte index = 0;
    while (mod.available() && index < NUMBER_BYTES_RESPONES)
    {
      sensorResponse[index] = mod.read();
      Serial.print(sensorResponse[index], HEX); // Print the received byte in HEX format
      Serial.print(" ");
      index++;
    }
    Serial.println(" $End of Rx data");
    getSoilSensorParameter();
    printSoilParameters();
  }
  else
  {
    // Print error message if no valid response received
    Serial.println("Sensor timeout or incomplete frame");
  }
  delay(1000);
}
