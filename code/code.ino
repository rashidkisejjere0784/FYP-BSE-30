#include <EEPROM.h>
#include "GravityTDS.h"
#include <LiquidCrystal.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <SoftwareSerial.h> // sim module

#define SensorPin 8
#define ONE_WIRE_BUS 4
#define TdsSensorPin A0

unsigned long int avgValue;
float b;
int buf[10], temp;
float tdsValue = 0;

SoftwareSerial mySerial(9,10); // RX, TX pins for SIM900a
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
GravityTDS gravityTds;


// JSON payload builder
String buildSensorPayload(float temperature, float phValue, float turbidityVoltage, float conductivity) {
  String payload = "{";
  payload += "\"temp\":" + String(temperature, 2) + ",";       // 2 decimal places
  payload += "\"pH\":" + String(phValue, 2) + ",";
  payload += "\"turbidity\":" + String(turbidityVoltage, 2) + ",";
  payload += "\"conductivity\":" + String(conductivity, 2);
  payload += "}";
  return payload;
}


void setup() {
  Serial.begin(9600);
  // Baud rate: 9600
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(1024);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTds.begin();  //initialization
  sensors.begin(); // begin temperature sensor

  // setup sim900
  mySerial.begin(9600);
  delay(2000);
}


void loop() {

  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);

  Serial.print("Temperature is: "); 
  Serial.print(temperature);
  Serial.println("");

  gravityTds.setTemperature(sensors.getTempCByIndex(0));  // set the temperature and execute temperature compensation
  gravityTds.update();  //sample and calculate
  tdsValue = gravityTds.getTdsValue();  // then get the value
  
  Serial.print(tdsValue,0);
  Serial.println("ppm");
  

  int sensorValue = analogRead(A3);  // Read the input on analog pin 0
  // Convert the analog reading (which goes from 0 to 1023) to a voltage (0 to 5V)
  float voltage = sensorValue * (5.0 / 1024.0);

  Serial.println(voltage); // Print the voltage value

  // Check turbidity levels
  if (voltage <= 2.5) {
    Serial.println("Turbidity > 3000");
  } else if (voltage > 2.5 && voltage <= 3) {
    Serial.println("Turbidity b/w 30```00 - 2700");
  } else if (voltage > 3 && voltage <= 3.5) {
    Serial.println("Turbidity b/w 2700 - 2000");
  } else if (voltage > 3.5 && voltage <= 4.25) {    
    Serial.println("Turbidity b/w 2000 - 750");
  } else if (voltage > 4.25) {
    Serial.println("No-Turbidity");
  } else {
    Serial.println("Error in reading");
  }

  // PH Sensor
  readSensorData();
  sortBuffer();
  float phValue = calculatePH();
  Serial.print("    pH:");
  Serial.print(phValue, 2);
  Serial.println(" ");
  digitalWrite(13, HIGH);
  delay(800);
  digitalWrite(13, LOW);

  delay(1500); // Delay to avoid excessive serial output
  // setup json object to send json data to the backend
  // need to implement conductivity sensor
  String payload = buildSensorPayload(temperature, phValue, voltage, temperature);
  makePostRequest(payload);
  delay(9000);
}

void readSensorData() {
  for (int i = 0; i < 10; i++) {
    buf[i] = analogRead(SensorPin);
    delay(10);
  }
}

void sortBuffer() {
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buf[i] > buf[j]) {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
}

float calculatePH() {
  avgValue = 0;
  for (int i = 2; i < 8; i++) {
    avgValue += buf[i];
  }
  float phValue = (float)avgValue * 5.0 / 1024 / 6;
  return 3.5 * phValue;
}

// Helper function for AT commands with response logging
void sendATCommand(const String& cmd, unsigned long timeout) {
  Serial.print("AT CMD: ");
  Serial.println(cmd);
  mySerial.println(cmd);
  
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (mySerial.available()) {
      Serial.write(mySerial.read()); // Echo module responses
    }
  }
}

// Sending data to the backend using http
void makePostRequest(const String jsonPayload) {
  // Debug output
  Serial.print("Sending JSON: ");
  Serial.println(jsonPayload);

  // Configure GPRS
  sendATCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 1000);
  sendATCommand("AT+SAPBR=3,1,\"APN\",\"internet\"", 1000); // Replace APN
  sendATCommand("AT+SAPBR=1,1", 2000);

  // HTTP Setup
  sendATCommand("AT+HTTPINIT", 1000);
  sendATCommand("AT+HTTPPARA=\"CID\",1", 500);
  sendATCommand("AT+HTTPPARA=\"URL\",\"http://f91f-102-134-149-100.ngrok-free.app/data\"", 500);
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 500);
  
  // POST JSON to server
  String dataCmd = "AT+HTTPDATA=" + String(jsonPayload.length()) + ",10000";
  sendATCommand(dataCmd, 500);
  sendATCommand(jsonPayload.c_str(), 2000); // Send the actual JSON

  // Execute POST and read response
  sendATCommand("AT+HTTPACTION=1", 5000);
  sendATCommand("AT+HTTPREAD", 1000);

  // Cleanup
  sendATCommand("AT+HTTPTERM", 500);
  sendATCommand("AT+SAPBR=0,1", 500);
}