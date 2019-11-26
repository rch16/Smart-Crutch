#include <ESP8266WiFi.h>
#include <Wire.h>
#include <WiFiClientSecure.h>
#include "FS.h"

#define SENSOR_SAMPLE_INTERVAL 10000 // Interval between individual IMU/Weight samples in microseconds
#define SENSOR_SAMPLE_SIZE 20 // Number of sensor samples in each period
#define DAILY_SAMPLES 5 // Number of gait samples per day

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
}

bool connectToSecureHost(BearSSL::WiFiClientSecure *client, const char* host, const int port) {
  int response = client->connect(host, port);
  if (!response) {
    Serial.print("Connection to ");
    Serial.print(host);
    Serial.print(" failed: Returned code ");
    Serial.println(response);
    return false;
  } else {
    Serial.println("Connection success");
  }
  return true;
}

bool connectToInsecureHost(WiFiClient *client, const char* host, const int port) {
  int response = client->connect(host, port);
  if (!response) {
    Serial.print("Connection to ");
    Serial.print(host);
    Serial.print(" failed: Returned code ");
    Serial.println(response);
    return false;
  } else {
    Serial.println("Connection success");
  }
  return true;
}

bool sendData(String line) {
  WiFiClientSecure client;
  client.setInsecure();
  Serial.print("Connecting to ");
  Serial.println(host);
  if (!connectToSecureHost(&client, host, httpsPort)) {
    Serial.println("Failed to connect to script host");
    return false;
  }

  String url = String("/macros/s/" + SCRIPT_ID + "/exec?data=" + line);
  Serial.print("requesting URL: ");
  Serial.println(url);

  //get the sensor data in script format using get method
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");

  if (!client.find("HTTP/1.1")) // skip HTTP/1.1
    return false;
  int st = client.parseInt(); // parse status code
  if (st == 200)
    return true;
  return false;
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

String getCurrentTimestamp() {
  WiFiClient client;
  String response = "";
  Serial.print("Connecting to worldclockapi.com");
  if (!connectToInsecureHost(&client, "worldclockapi.com", 80)) {
    return "Error connecting to worldclockapi.com";
  }
  client.println("GET /api/json/utc/now HTTP/1.1\r\nHost: worldclockapi.com\r\n");
  client.println("Connection: close");
  client.println();
  String line = client.readStringUntil('"currentDateTime"');
  line = client.readStringUntil(',');
  line = client.readStringUntil(',');
  Serial.println(line);
  return(line);
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

  // Store initial time
  oldTime = micros();
  startTime = micros();
}

bool uploadDatetimeMillis() {
  Serial.println("Uploading new datetime");
  if(sendData(getCurrentTimestamp() + ",Arduino_milliseconds=" + millis()))
    Serial.println("Datetime upload successful");
    return true;
  return false;
}

bool crutchInUse() {
  // Todo add weight sensor measurements to detect when crutch is in use
  return true;
}

bool collectGaitSample() {
  File appendLog = SPIFFS.open("/log.csv", "a");
  appendLog.print("Start_of_sample|");

  for(int n = 0; n <= SENSOR_SAMPLE_SIZE; n++) {
    newTime = micros();
    while((newTime - oldTime) < SENSOR_SAMPLE_INTERVAL){
      newTime = micros();
    }
    oldTime = micros();
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

    appendLog.print(millis());
    appendLog.print(',');
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
  }
  appendLog.print("End_of_sample|");
  appendLog.close();
}

bool wifiConnected() {
  WiFiClient client;
  String response = "";
  Serial.print("Connecting to google.com");
  if (!connectToInsecureHost(&client, "google.com", 80)) {
    return false;
  }
  return true;
}

void clearData() {
  File writeLog = SPIFFS.open("/log.csv", "w");
  writeLog.close();
}

bool uploadNewData() {
  Serial.println("Uploading new data");
  if (!sendData("New_upload"))
    return false;
  if (!uploadDatetimeMillis())
    return false;
  File readLog = SPIFFS.open("/log.csv", "r");
  String line = readLog.readStringUntil('|');
  while(line.length() > 0) {
    Serial.println(line);
    if (!sendData(line))
      return false;
    line = readLog.readStringUntil('|');
  }
  if (!sendData("Upload end"))
    return false;
  readLog.close();
  clearData();
  return true;
}

int samples_today = 0;
bool uploaded_time_stamp = false;
bool uploaded_new_data = false;

void loop() {
  if (crutchInUse() && samples_today < DAILY_SAMPLES) { // If weight is being applied to the crutch and we haven't collected more than DAILY_SAMPLES
    collectGaitSample();
    samples_today++;
    Serial.print("Sample collected, samples today = ");
    Serial.println(samples_today);
    uploaded_new_data = false;
  }
  
  if (!uploaded_new_data) { // If new data exists but hasn't been uploaded yet
    if (!wifiConnected()) { // Check if wifi connection exists by connecting to google
      connectToWifi(); 
    }
    if (wifiConnected()) {  
      uploaded_new_data = uploadNewData();
    }
  }

}
