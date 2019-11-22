#include <ESP8266WiFi.h>
#include <Wire.h>
#include <WiFiClientSecure.h>
#include "FS.h"
 
#define MPU9250_ADDRESS 0x68
#define MAG_ADDRESS 0x0C
 
#define GYRO_FULL_SCALE_250_DPS 0x00 
#define GYRO_FULL_SCALE_500_DPS 0x08
#define GYRO_FULL_SCALE_1000_DPS 0x10
#define GYRO_FULL_SCALE_2000_DPS 0x18
 
#define ACC_FULL_SCALE_2_G 0x00 
#define ACC_FULL_SCALE_4_G 0x08
#define ACC_FULL_SCALE_8_G 0x10
#define ACC_FULL_SCALE_16_G 0x18

const char* host = "script.google.com";
const int httpsPort = 443; 
String SCRIPT_ID = "AKfycbyt1zJXaOvHo2_cz7Mfp6ivhan6XcGtnQO_UQzGzq6ECh3G4Zgj";

const String SSIDS[2] = {"Fellas WiFi", "Closed Network"};
const String PASSWORDS[2] = {"Silverton4ever", "portugal1"};
const int NUM_NETWORKS = 2;
 
const char* ssid     = "Fellas WiFi";
const char* password = "Silverton4ever";

// Initial time
long oldTime;
long newTime;
long startTime;

void connectToWifi() {
  WiFi.scanNetworks(false, false);
  Serial.println("Scanning for networks");
  while (WiFi.scanComplete() < 0) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("Found:");
  int numberOfAccessPoints = WiFi.scanComplete();
  for(int n = 0; n < numberOfAccessPoints; n++) {
    Serial.println(WiFi.SSID(n));
  }

  bool connected = false;

  for(int n = 0; n < numberOfAccessPoints; n++) {
    for(int m = 0; m < NUM_NETWORKS; m++) {
      if (SSIDS[m] == WiFi.SSID(n)) {
        connected = true;
        Serial.print("Connecting to ");
        Serial.print(SSIDS[m]);
        WiFi.begin(SSIDS[m], PASSWORDS[m]);
        while (WiFi.status() != WL_CONNECTED) {
          Serial.print(".");
          delay(500);
        }
        Serial.println(".");
        Serial.println("WiFi connected");
      }
    }
  }
  if(!connected) {
    Serial.println("WiFi not connected");
    connectToWifi();
  }
}

void sendData(String line) {
  WiFiClientSecure client;
  client.setInsecure();
  Serial.print("Connecting to ");
  Serial.println(host);
  int response = client.connect(host, httpsPort);
  if (!response) {
    Serial.print("Connection failed: Returned code ");
    Serial.println(response);
    return;
  } else {
    Serial.println("Connection success");
  }


  String url = String("/macros/s/" + SCRIPT_ID + "/exec?data=" + line);
  Serial.print("requesting URL: ");
  Serial.println(url);

  //get the sensor data in script format using get method
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");

  Serial.println("request sent");
}

// This function read Nbytes bytes from I2C device at address Address. 
// Put read bytes starting at register Register in the Data array. 
void I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t* Data) {
  // Set register address
  Wire.beginTransmission(Address);
  Wire.write(Register);
  Wire.endTransmission();
  
  // Read Nbytes
  Wire.requestFrom(Address, Nbytes);
  uint8_t index=0;
  while (Wire.available())
  Data[index++]=Wire.read();
}

// Write a byte (Data) in device (Address) at register (Register)
void I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data) {
  // Set register address
  Wire.beginTransmission(Address);
  Wire.write(Register);
  Wire.write(Data);
  Wire.endTransmission();
}
 
void setup() {
  Serial.begin(115200);
  connectToWifi();
  Wire.begin();
  SPIFFS.begin();
  
  // Set accelerometers low pass filter at 5Hz
  I2CwriteByte(MPU9250_ADDRESS,29,0x06);
  // Set gyroscope low pass filter at 5Hz
  I2CwriteByte(MPU9250_ADDRESS,26,0x06);
  
  
  // Configure gyroscope range
  I2CwriteByte(MPU9250_ADDRESS,27,GYRO_FULL_SCALE_1000_DPS);
  // Configure accelerometers range
  I2CwriteByte(MPU9250_ADDRESS,28,ACC_FULL_SCALE_4_G);
  // Set by pass mode for the magnetometers
  I2CwriteByte(MPU9250_ADDRESS,0x37,0x02);
  
  // Request continuous magnetometer measurements in 16 bits
  I2CwriteByte(MAG_ADDRESS,0x0A,0x16);
  
  pinMode(13, OUTPUT);
  
  Serial.println("Starting in 2 seconds");
  delay(2000);

  
  // Store initial time
  oldTime = micros();
  startTime = micros();
}

String line;

// Counter
long int cpt=0;

void loop() {
  if (cpt >= 200) {
    Serial.println("START IF");
    Serial.println(cpt);
    File readLog = SPIFFS.open("/log.csv", "r");
    line = readLog.readStringUntil('|');
    while(line.length() > 0) {
      Serial.println("Here1");
      Serial.println(line);
      sendData(line);
      line = readLog.readStringUntil('|');
    }
    sendData("------------------");
    readLog.close();
    File writeLog = SPIFFS.open("/log.csv", "w");
    writeLog.close();
    Serial.println("Here2");
    cpt = 0;
  }
  

  
  cpt++;
  //wait for a bit, 10ms
  newTime = micros();
  while((newTime - oldTime) < 10000){
    newTime = micros();
  }
  oldTime = micros();

// ____________________________________
// ::: accelerometer and gyroscope :::
 
// Read accelerometer and gyroscope
uint8_t Buf[14];
I2Cread(MPU9250_ADDRESS,0x3B,14,Buf);
 
// Create 16 bits values from 8 bits data
 
// Accelerometer
int16_t ax=-(Buf[0]<<8 | Buf[1]);
int16_t ay=-(Buf[2]<<8 | Buf[3]);
int16_t az=Buf[4]<<8 | Buf[5];
 
// Gyroscope
int16_t gx=-(Buf[8]<<8 | Buf[9]);
int16_t gy=-(Buf[10]<<8 | Buf[11]);
int16_t gz=Buf[12]<<8 | Buf[13];


File appendLog = SPIFFS.open("/log.csv", "a");
appendLog.print(micros());
appendLog.print(',');
appendLog.print(ax);
appendLog.print(',');
appendLog.print(ay);
appendLog.print(',');
appendLog.print(az);
appendLog.print(',');
appendLog.print(gx);
appendLog.print(',');
appendLog.print(gy);
appendLog.print(',');
appendLog.print(gz);
appendLog.print('|');
appendLog.close();
 

 
 
// End of line
Serial.println("");
// delay(100); 

  // sendData();


}
