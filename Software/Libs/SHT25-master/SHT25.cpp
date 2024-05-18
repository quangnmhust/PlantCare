/*
  Author : Amit Ate
  email  : amitate007@gmail.com
  Description: Sensirion sensor SHT25 is tested with this library on Arduino Uno successefully
*/

#include<SHT25.h>
#include<Wire.h>

SHT25::SHT25(void){
}

char SHT25::begin(void){
  Wire.begin();
  if(resetSensor()){
    return 1;
  }else{return 0;}
}

char SHT25::resetSensor(void){
  Wire.beginTransmission(SHT25_ADDR);
  Wire.write(SOFT_RESET);
  char error = Wire.endTransmission();
  if(error == 0){
    delay(15);    //wait for sensor to reset
    return 1;
  }else{return 0;}
}

float SHT25::getHumidity(void){
  if(readBytes(RH_NO_HOLD, S_RH, 3, is_RH)){
    RH = -6.0 + 125.0*(S_RH/((long)1<<16));
    return RH;
  }else{return RH_ERROR;}
}

float SHT25::getTemperature(){
  if(readBytes(T_NO_HOLD, S_T, 3, is_TEMP)){
    TEMP = -46.85 + 175.72*(S_T/((long)1<<16));
    return TEMP;
  }else{return T_ERROR;}
}

char SHT25::readBytes(char CMD, float &value, char length, char param){
  unsigned char data[length];
  Wire.beginTransmission(SHT25_ADDR);
  Wire.write(CMD);
  char error = Wire.endTransmission();
  if(param){
    delay(RH_Delay);
  }else{delay(T_Delay);}

  if(error == 0){
    Wire.requestFrom(SHT25_ADDR, length);
    while (!Wire.available());
    for(char x=0; x<length; x++){
      data[x] = Wire.read();
      x++;
    }
    value = (float)((unsigned int)data[0]*((int)1<<8) + (unsigned int)(data[1]&((int)1<<2)));
    return 1;
  }else{return 0;}
}

char SHT25::readByte(char CMD, byte &value){
  Wire.beginTransmission(SHT25_ADDR);
  Wire.write(CMD);
  char error = Wire.endTransmission();
  if(error == 0){
    Wire.requestFrom(SHT25_ADDR,1);
    while (!Wire.available()) {
      value = Wire.read();
    }
    return 1;
  }else{return 0;}
}

char SHT25::writeByte(char CMD, byte regDat){
  Wire.beginTransmission(SHT25_ADDR);
  Wire.write(CMD);
  Wire.write(regDat);
  char error = Wire.endTransmission();
  if(error == 0){
    return 1;
  }else{return 0;}
}

char SHT25::setResCombination(byte combination){
  byte regDat, value;
  switch (combination) {
    case 1:                 //RH-12bit, T-14bit
      this->T_Delay   = 85;
      this->RH_Delay  = 29;
      regDat       = 0x02;
      break;
    case 2:                 //RH-8bit, T-12bit
      this->T_Delay   = 22;
      this->RH_Delay  = 4;
      regDat       = 0x03;
      break;
    case 3:                 //RH-10bit, T-13bit
      this->T_Delay   = 43;
      this->RH_Delay  = 9;
      regDat       = 0x82;
      break;
    case 4:                 //RH-11bit, T-11bit
      this->T_Delay   = 11;
      this->RH_Delay  = 15;
      regDat       = 0x83;
      break;
    default:
      break;
  }
  if(readByte(R_UREG, value)){
    regDat |= (value & 0x7E);
    if(writeByte(W_UREG, regDat)){
      return 1;
    }else{return 0;}
  }else{return 0;}
}

char SHT25::enableHeater(void){
  byte value, data;
  if(readByte(R_UREG, value)){
    data = (value & 0xFB) | 0x04;
    if(writeByte(W_UREG, data)){
      return 1;
    } else{return 0;}
  }else{return 0;}
}


char SHT25::disableHeater(void){
  byte value, data;
  if(readByte(R_UREG, value)){
    data = (value & 0xFB);
    if(writeByte(W_UREG, data)){
      return 1;
    } else{return 0;}
  }else{return 0;}
}

char SHT25::isEOB(void){
  byte value;
  if(readByte(R_UREG, value)){
    value &= 0x40;
    if(value == 0x40){
      return 1;
    } else{return 0;}
  }else{return 0;}
}
