#include<SHT25.h>

SHT25 H_Sens;

void setup(void){
  Serial.begin(9600);
  if(H_Sens.begin()){
    Serial.println("SHT25 initialization successeful!");
  }else{
    Serial.println("Initialization failed check for connections!");
  }
}

void loop(void){
  Serial.print("Humidity    : ");
  Serial.print(H_Sens.getHumidity());
  Serial.println(" %RH");
  Serial.print("Temperature : ");
  Serial.print(H_Sens.getTemperature());
  Serial.println(" C");

  delay(1000);
}
