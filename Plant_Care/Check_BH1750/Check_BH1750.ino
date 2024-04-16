void setup() {
  // start communication with baud rate 9600
  Serial.begin(9600);   // Serial Monitor
  Serial2.begin(4800);  // RS485

  // wait a moment to allow serial ports to initialize
  delay(100);
}

void loop() {
  // Check if there's data available on Serial
  if (Serial2.available()) {
    char data = Serial2.read();  // read the received character
    Serial.print(data);          // print the recived data to Serial Monitor
  }
}
