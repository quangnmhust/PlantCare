#include <SoftwareSerial.h>

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


 
void setup()
{
  Serial.begin(9600); // Initialize serial communication for debugging
  mod.begin(4800);    // Initialize software serial communication at 4800 baud rate


  // Serial3.begin(4800);
  delay(500);
}
 
 
void loop()
{
 
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
  delay(1000);
}
