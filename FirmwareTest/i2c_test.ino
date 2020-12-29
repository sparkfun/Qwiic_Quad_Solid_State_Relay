#include <Wire.h>

void setup() {
   Wire.begin();
   Serial.begin(9600);  // start serial for output
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
}

void send(uint8_t cmd) {
  Serial.print("Sending ");
  Serial.println(cmd);
  Wire.beginTransmission(8); // transmit to device #4
  Wire.write(cmd);              // sends one byte 
  Wire.endTransmission();    // stop transmitting
}
void send2(uint8_t cmd, uint8_t arg1) {
  Serial.print("Sending2 ");
  Serial.print(cmd);
  Serial.print(" ");
  Serial.println(arg1);
  Wire.beginTransmission(8); // transmit to device #4
  Wire.write(cmd);              // sends one byte 
  Wire.write(arg1);              // sends one byte 
  Wire.endTransmission();    // stop transmitting
}

void read(uint8_t reg) {
  char buf[12];
  
  Serial.print("Sending ");
  Serial.println(reg);
  Wire.beginTransmission(8); // transmit to device #4
  Wire.write(reg);              // sends one byte 
  Wire.endTransmission();    // stop transmitting
  
  Serial.print("Receiving ");
  Wire.requestFrom(8, 1);    // request 6 bytes from slave device #8
  while (Wire.available()) { // slave may send less than requested
    char c = Wire.read(); // receive a byte as character
    Serial.print(itoa(c, buf, 10));         // print the character
  }
  Serial.println();
}

void loop() {
  onofftest();
  pwmtest();

  Serial.println("Firmware version:");
  read(0x30);
}

void pwmtest() {
  Serial.println("All OFF");
  send(0x0A);
  delay(1000);

  Serial.println("PWM!");
  send2(0x10, 100);
  send2(0x11, 80);
  send2(0x12, 40);
  send2(0x13, 20);

  read(0x10);
  read(0x11);
  read(0x12);
  read(0x13);
  delay(4000);
  
  Serial.println("All OFF");
  send(0x0A);
  delay(1000);

  
  Serial.println("PWM and regular 3+4 ON");
  send2(0x10, 100); // 1
  send2(0x11, 80); // 2
  send(0x03); // 3
  delay(250);
  send(0x04); // 4
  delay(250);
  delay(4000);

  Serial.println("3 OFF");  
  send(0x03); // 3
  delay(4000);
  
  Serial.println("2 ON");
  send(0x02); // 2
  send(0x02); // 2
  delay(4000);
}

void onofftest() {
  Serial.println("Should be all OFF");
  read(0x05);
  read(0x06);
  read(0x07);
  read(0x08);

  send(0x01);
  delay(250);
  send(0x02);
  delay(250);
  send(0x03);
  delay(250);
  send(0x04);
  delay(250);

  Serial.println("Should be all ON");
  read(0x05);
  read(0x06);
  read(0x07);
  read(0x08);

  Serial.println("All OFF");
  send(0x0A);
  delay(1000);
  Serial.println("All ON");
  send(0x0B);
  delay(1000);
  Serial.println("toggle all OFF");
  send(0x0C);
  delay(1000);
  
  Serial.println("toggle all ON");
  send(0x0C);
  delay(1000);
  
  Serial.println("toggle all OFF");
  send(0x0C);
  delay(1000);
}
