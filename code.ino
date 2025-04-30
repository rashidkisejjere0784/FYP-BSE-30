#include <EEPROM.h>
#include "GravityTDS.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

// Pin definitions
#define SensorPin    8     // pH sensor pin (analog)
#define ONE_WIRE_BUS 4     // Temperature sensor pin (digital)
#define TdsSensorPin A0    // TDS sensor pin (analog)
#define TurbidityPin A5    // Turbidity sensor pin (analog)

// Sensor objects and variables
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
GravityTDS gravityTds;
float tdsValue       = 0.0;
float phValue        = 0.0;
float turbidityVoltage = 0.0;
float temperature    = 0.0;
unsigned long avgValue;
int buf[10], temp;

// SIM800L configuration (SoftwareSerial: TX->7, RX->8)
SoftwareSerial SIM800L(7, 8);

// Server and network settings
const char* SERVER   = "fyp-bse30-backend.onrender.com";
const char* PATH     = "/api/read_data";
const char* APN      = "internet";
const char* USERNAME = "";
const char* PASSWORD = "";

void setup() {
  Serial.begin(9600);
  Serial.println("\n\nSystem Boot Sequence Started");

  // Initialize sensors
  Serial.println("Initializing TDS Sensor...");
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);
  gravityTds.setAdcRange(1024);
  gravityTds.begin();

  Serial.println("Starting Temperature Sensor...");
  sensors.begin();

  // Initialize SIM800L module
  Serial.println("Initializing SIM800L Module...");
  SIM800L.begin(9600);   
  delay(3000);        

  // Test basic communication
  Serial.println("Testing AT command...");
  clearSIMBuffer();
  SIM800L.println("AT");
  delay(3000);
  printSIMResponse();

  // Check signal quality
  sendATCommand("AT+CSQ", 3000, "Check Signal Quality");

  // Attach to GPRS
  if (attachToGPRS()) {
    Serial.println("Attached to GPRS successfully");
  } else {
    Serial.println("Failed to attach to GPRS");
  }

  pinMode(13, OUTPUT);  // LED indicator
}

void loop() {
  // Read temperature
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" °C");

  // Read TDS
  gravityTds.setTemperature(temperature);
  gravityTds.update();
  tdsValue = gravityTds.getTdsValue();
  Serial.print("TDS/Conductivity: ");
  Serial.print(tdsValue, 0);
  Serial.println(" ppm");

  // Read turbidity
  int sensorValue = analogRead(TurbidityPin);
  turbidityVoltage = sensorValue * (5.0 / 1024.0);
  Serial.print("Turbidity Voltage: ");
  Serial.println(turbidityVoltage, 2);
  // classification...
  if      (turbidityVoltage <= 2.5)  Serial.println("Turbidity > 3000");
  else if (turbidityVoltage <= 3.0)  Serial.println("Turbidity 2700–3000");
  else if (turbidityVoltage <= 3.5)  Serial.println("Turbidity 2000–2700");
  else if (turbidityVoltage <= 4.25) Serial.println("Turbidity 750–2000");
  else                               Serial.println("No turbidity");

  // Read and calculate pH
  readSensorData();
  sortBuffer();
  phValue = calculatePH();
  Serial.print("pH: ");
  Serial.println(phValue, 2);

  // LED blink
  digitalWrite(13, HIGH);
  delay(800);
  digitalWrite(13, LOW);

  // Send data
  Serial.println("Sending data online");
  sendDataOnline();

  delay(15000);  // 15 s between cycles
}

// Clear any pending bytes from SIM800L
void clearSIMBuffer() {
  while (SIM800L.available()) SIM800L.read();
}

// Print whatever SIM800L returns to Serial
void printSIMResponse() {
  while (SIM800L.available()) {
    Serial.write(SIM800L.read());
  }
  Serial.println();
}

// Attach to GPRS
bool attachToGPRS() {
  String apnCmd = "AT+CSTT=\"" + String(APN) + "\",\"" + String(USERNAME) + "\",\"" + String(PASSWORD) + "\"";
  if (!sendATCommand(apnCmd, 3000, "Set APN")) return false;

  int retries = 3;
  while (retries--) {
    if (sendATCommand("AT+CGATT=1", 10000, "Attach GPRS")) {
      return true;
    }
    delay(2000);
  }
  return false;
}

// Send an AT command and wait for OK
bool sendATCommand(const String& cmd, int timeout, const String& desc) {
  clearSIMBuffer();
  Serial.print("[AT] "); Serial.println(desc);
  SIM800L.println(cmd);

  unsigned long start = millis();
  String resp;
  while (millis() - start < timeout) {
    while (SIM800L.available()) {
      char c = SIM800L.read();
      resp += c;
      if (resp.endsWith("OK\r\n")) {
        Serial.println("<< " + resp);
        return true;
      }
      if (resp.endsWith("ERROR\r\n")) {
        Serial.println("<< " + resp);
        return false;
      }
    }
  }
  Serial.println("!! Timeout waiting for OK");
  Serial.println("<< " + resp);
  return false;
}

// Open TCP, send HTTP POST, close
void sendDataOnline() {
  String payload = "{\"temperature\":" + String(temperature,1)
                 + ",\"tds\":" + String(tdsValue,0)
                 + ",\"turbidity\":" + String(turbidityVoltage,2)
                 + ",\"ph\":" + String(phValue,2) + "}";

  // Start TCP
  SIM800L.println("AT+CIPSTART=\"TCP\",\"" + String(SERVER) + "\",80");
  delay(2000);
  printSIMResponse();

  // Prepare HTTP request
  String req = "POST " + String(PATH) + " HTTP/1.1\r\n"
             + "Host: " + String(SERVER) + "\r\n"
             + "Content-Type: application/json\r\n"
             + "Content-Length: " + String(payload.length()) + "\r\n\r\n"
             + payload;

  // Send data length
  SIM800L.println("AT+CIPSEND=" + String(req.length()));
  delay(100);
  if (SIM800L.find("> ")) {
    SIM800L.print(req);
    Serial.println(">> Sent HTTP request");
  } else {
    Serial.println("!! No prompt from SIM800L");
    return;
  }

  delay(5000);
  printSIMResponse();
  SIM800L.println("AT+CIPCLOSE");
  delay(500);
  printSIMResponse();
}

// pH sensor helpers
void readSensorData(){
  for(int i=0;i<10;i++){
    buf[i]=analogRead(SensorPin);
    delay(10);
  }
}
void sortBuffer(){
  for(int i=0;i<9;i++) for(int j=i+1;j<10;j++){
    if(buf[i]>buf[j]){ temp=buf[i]; buf[i]=buf[j]; buf[j]=temp; }
  }
}
float calculatePH(){
  avgValue = 0;
  for(int i=2;i<8;i++) avgValue += buf[i];
  float v = (float)avgValue * 5.0 / 1024.0 / 6.0;
  return 3.5 * v;  // calibration may be required
}
