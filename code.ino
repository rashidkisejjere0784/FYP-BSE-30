#include <EEPROM.h>
#include "GravityTDS.h"
#include <LiquidCrystal.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#define SensorPin 8

unsigned long int avgValue;
float b;
int buf[10], temp;

#define ONE_WIRE_BUS 4
#define TdsSensorPin A0

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float tdsValue = 0;

GravityTDS gravityTds;

void setup() {
  Serial.begin(9600);
  // Baud rate: 9600
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(1024);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTds.begin();  //initialization
  sensors.begin(); // begin temperature sensor
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
}



// #include <EEPROM.h>
// #include "GravityTDS.h"

// #define TdsSensorPin A1
// GravityTDS gravityTds;

// float temperature = 25,tdsValue = 0;

// void setup()
// {
//     Serial.begin(9600);
//     gravityTds.setPin(TdsSensorPin);
//     gravityTds.setAref(5.0);  //reference voltage on ADC, default 5.0V on Arduino UNO
//     gravityTds.setAdcRange(1024);  //1024 for 10bit ADC;4096 for 12bit ADC
//     gravityTds.begin();  //initialization
// }

// void loop()
// {
//     //temperature = readTemperature();  //add your temperature sensor and read it
//     gravityTds.setTemperature(temperature);  // set the temperature and execute temperature compensation
//     gravityTds.update();  //sample and calculate
//     tdsValue = gravityTds.getTdsValue();  // then get the value
//     Serial.print(tdsValue,0);
//     Serial.println("ppm");
//     delay(1000);
// }
