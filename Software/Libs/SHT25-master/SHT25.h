/*
  File   : SHT25.h
  Author : Amit Ate
  Email  : amit@uravulabs.com
  Company: Uravu Labs
  Note: Library only support NO HOLD master communication
*/

#include <Arduino.h>

//Commands
#define T_NO_HOLD     0xF3 //trigger T measurement with no hold master
#define RH_NO_HOLD    0xF5 //trigger RH measurement with no hold master
#define W_UREG        0xE6 //write user registers
#define R_UREG        0xE7 //read user registers
#define SOFT_RESET    0xFE //soft reset

#define is_RH         0x01
#define is_TEMP       0x00


class SHT25{
  public:
    SHT25(void);
    char begin(void);
    char setResCombination(byte combination);
    char enableHeater(void);
    char disableHeater(void);
    float getTemperature(void);
    float getHumidity(void);
    char isEOB(void);
    const float RH_ERROR = 101.0,T_ERROR = 126.0;

  private:
    const byte SHT25_ADDR = 0x40; //Sensor addresses
    byte T_Delay = 85;  //for 14 bit resolution
    byte RH_Delay = 29; //for 12 bit resolution
    float TEMP, RH, S_T, S_RH;
    char resetSensor(void);
    char readBytes(char CMD, float &value, char length, char param);
    char readByte(char CMD, byte &value);
    char writeByte(char CMD, byte regDat);
};
