#include "GravityTDS.h"
#include <LiquidCrystal.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <SoftwareSerial.h> // sim module
#include <ArduinoJson.h>
#include <Arduino.h>

#define TEMP_SENSOR_PIN 4
#define TDS_SENSOR_PIN A0
#define TURBIDITY_SENSOR_PIN A5
#define PH_SENSOR_PIN A1

StaticJsonBuffer<200> jsonBuffer; 

const String endpointURL = "http://3f0e-102-134-149-100.ngrok-free.app/data";


SoftwareSerial mySerial(9,10); // RX, TX pins for SIM900a
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temp_sensor(&oneWire);

GravityTDS gravityTds; // TDS

void setup() {
  Serial.begin(9600);

  temp_sensor.begin(); // begin temperature sensor
  temp_sensor.setResolution(11);

  // TDS
    gravityTds.setPin(TDS_SENSOR_PIN);
    gravityTds.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
    gravityTds.setAdcRange(1024);  //1024 for 10bit ADC;4096 for 12bit ADC
    gravityTds.begin(); 

  // setup sim900
  mySerial.begin(9600);
  delay(2000);

   DynamicJsonBuffer jsonBuffer;
}


void loop() {

  float temp_value = readTemperature(); // this works just fine
  delay(1500);
  Serial.print("Temp: ");
  Serial.print(temp_value);
  Serial.println();

// tds sesnor
  float tds_value = readTDS(temp_value);  // then get the value
  Serial.print(tds_value,0);
  Serial.println("ppm");
  delay(1000);

  // turbidity sensor
  int turbidity_value = analogRead(TURBIDITY_SENSOR_PIN);
  float voltage = turbidity_value * (5.0 / 1024.0);
 
  Serial.println ("Sensor Output (V):");
  Serial.println (voltage);
  Serial.println();
  delay(1000);

  float ph_value = readPH();
  Serial.print("pH: ");
  Serial.println(ph_value, 2);

  // send data to server
  sendSensorData(ph_value, voltage, temp_value, tds_value);
}

float readTemperature()
{
  temp_sensor.requestTemperatures();
  delay(1500);
  return temp_sensor.getTempCByIndex(0);
}

float readTDS(float temp_compensation)
{
  gravityTds.setTemperature(temp_compensation);  // set the temperature and execute temperature compensation
  gravityTds.update();  //sample and calculate
  return gravityTds.getTdsValue(); 
}

void ShowSerialData()
{
  while (mySerial.available() != 0)
    Serial.write(mySerial.read());
  delay(1000);
 
}

float readPH() {
  int buf[10];
  int sum = 0;

  for (int i = 0; i < 10; i++) {
    buf[i] = analogRead(PH_SENSOR_PIN);
    sum += buf[i];
    // Serial.print("Analog value: ");
    // Serial.println(buf[i]);
    // delay(100);
  }

  float avgValue = sum / 10.0;

  // Apply your calibration formula
  float phValue = 0.39 * avgValue - 13.1;

  Serial.print("Average Analog Value: ");
  Serial.println(avgValue);
  Serial.print("Estimated pH Value: ");
  Serial.println(phValue);

  return phValue;
}

// send data to server

void sendSensorData(float phValue, float turbidity, float temperature, float conductivity) {
  mySerial.println("AT");
  delay(3000);

  mySerial.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  delay(1500);
  ShowSerialData();

  mySerial.println("AT+SAPBR=3,1,\"APN\",\"internet\"");
  delay(1500);
  ShowSerialData();

  mySerial.println("AT+SAPBR=1,1");
  delay(1500);
  ShowSerialData();

  mySerial.println("AT+SAPBR=2,1");
  delay(1500);
  ShowSerialData();

  mySerial.println("AT+HTTPINIT");
  delay(1500);
  ShowSerialData();

  mySerial.println("AT+HTTPPARA=\"CID\",1");
  delay(1500);
  ShowSerialData();

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& object = jsonBuffer.createObject();

  object.set("ph", phValue);
  object.set("turbidity", turbidity);
  object.set("temperature", temperature);
  object.set("conductivity", conductivity);

  object.printTo(Serial);
  Serial.println(" ");
  String sendtoserver;
  object.prettyPrintTo(sendtoserver);
  delay(1500);

  mySerial.println("AT+HTTPPARA=\"URL\",\"" + endpointURL + "\"");
  delay(4000);
  ShowSerialData();

  mySerial.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(4000);
  ShowSerialData();

  mySerial.println("AT+HTTPDATA=" + String(sendtoserver.length()) + ",100000");
  Serial.println(sendtoserver);
  delay(6000);
  ShowSerialData();

  mySerial.println(sendtoserver);
  delay(4000);
  ShowSerialData();

  mySerial.println("AT+HTTPACTION=1");
  delay(4000);
  ShowSerialData();

  mySerial.println("AT+HTTPREAD");
  delay(1500);
  ShowSerialData();

  mySerial.println("AT+HTTPTERM");
  delay(1500);
  ShowSerialData();
}
