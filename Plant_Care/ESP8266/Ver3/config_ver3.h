

//RS485
#define ADDRESS 0x02
#define FUNCTION_CODE 0x03
#define INITIAL_ADDRESS_HI 0x00
#define INITIAL_ADDRESS_LO 0x00
#define DATA_LENGTH_HI 0x00
#define DATA_LENGTH_LO 0x0A
#define CHECK_CODE_LO 0xC5
#define CHECK_CODE_HI 0xFE
#define NUMBER_BYTES_RESPONES 25
const byte request[] = { ADDRESS, FUNCTION_CODE, INITIAL_ADDRESS_HI, INITIAL_ADDRESS_LO, DATA_LENGTH_HI, DATA_LENGTH_LO, CHECK_CODE_LO, CHECK_CODE_HI };
byte sensorResponse[NUMBER_BYTES_RESPONES];

//
const char* ssid = "Tom Bi";
const char* password =  "TBH123456";
// String ssid = "IOTTEST";
// String pss = "123456789";
// String ssid = "TTSV_Mobile";
// String pss = "ttsv@2020";
const int myChannelNumber = 2539374;
const char *myWriteAPIKey = "XHYKSL332GNQJZD9";
const char *mqttServer = "sanslab.ddns.net";
const int mqttPort = 1883;
const char *mqttUser = "admin";
const char *mqttPassword = "123";
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 6 * 3600;
const int daylightOffset_sec = 3600;
unsigned long lastMsg = 0;

uint8_t broadcastAddress[] = {0x3C, 0xE9, 0x0E, 0x8C, 0xCB, 0xE4};
// 3C:E9:0E:8C:CB:E4

unsigned long lastTime = 0;  
unsigned long timerDelay = 60000*6;  // send readings timer

typedef struct Data_manager {
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

float convertBytesToFloat(byte hi, byte low);
void getSoilSensorParameter(Data_t *data_soil);
void callback(char *topic, byte *payload, unsigned int length);

bool mutexFlag = false;
void lockMutex();
void unlockMutex();
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus);