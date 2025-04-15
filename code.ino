#include <EEPROM.h>
#include "GravityTDS.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

// Pin definitions
#define SensorPin 8          // pH sensor pin (analog)
#define ONE_WIRE_BUS 4       // Temperature sensor pin (digital)
#define TdsSensorPin A0      // TDS sensor pin (analog)
#define TurbidityPin A5      // Turbidity sensor pin (analog)

// Sensor objects and variables
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
GravityTDS gravityTds;
float tdsValue = 0.0;
float phValue = 0.0;
float turbidityVoltage = 0.0;
float temperature = 0.0;
unsigned long int avgValue;
int buf[10], temp;

// SIM900 configuration (SoftwareSerial: TX -> pin 7, RX -> pin 8)
SoftwareSerial SIM900(7, 8);

// Server and network settings
const char* SERVER = "fyp-bse30-backend.onrender.com";
const char* PATH = "/api/read_data";
const char* APN = "internet";      // Replace with your network provider's APN
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

    // Initialize SIM900 module
    Serial.println("Initializing SIM900 Module...");
    SIM900.begin(115200);  // Adjust baud rate if necessary (e.g., 9600, 115200)
    delay(20000);          // Wait for SIM900 to power up

    // Test basic communication with SIM900
    Serial.println("Testing AT command...");
    while (SIM900.available()) SIM900.read(); // Clear buffer
    SIM900.println("AT");
    delay(1000);
    while (SIM900.available()) {
        Serial.write(SIM900.read());
    }
    Serial.println();

    // Check signal quality
    sendATCommand("AT+CSQ", 3000, "Check Signal Quality");

    // Attempt to attach to GPRS
    if (attachToGPRS()) {
        Serial.println("Attached to GPRS successfully");
    } else {
        Serial.println("Failed to attach to GPRS");
    }

    pinMode(13, OUTPUT);  // LED indicator on pin 13
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
    if (turbidityVoltage <= 2.5) {
        Serial.println("Turbidity > 3000");
    } else if (turbidityVoltage <= 3.0) {
        Serial.println("Turbidity between 3000 and 2700");
    } else if (turbidityVoltage <= 3.5) {
        Serial.println("Turbidity between 2700 and 2000");
    } else if (turbidityVoltage <= 4.25) {
        Serial.println("Turbidity between 2000 and 750");
    } else {
        Serial.println("No turbidity");
    }

    // Read and calculate pH
    readSensorData();
    sortBuffer();
    phValue = calculatePH();
    Serial.print("pH: ");
    Serial.println(phValue, 2);

    // Blink LED to indicate sensor reading
    digitalWrite(13, HIGH);
    delay(800);
    digitalWrite(13, LOW);

    // Send data to server
    Serial.println("Sending data online");
    sendDataOnline();

    delay(15000);  // Wait before next cycle
}

// Attach to GPRS with retry mechanism
bool attachToGPRS() {
    String apnCommand = "AT+CSTT=\"" + String(APN) + "\",\"" + String(USERNAME) + "\",\"" + String(PASSWORD) + "\"";
    if (!sendATCommand(apnCommand, 3000, "Set APN")) {
        return false;
    }

    int retries = 3;
    while (retries > 0) {
        if (sendATCommand("AT+CGATT=1", 10000, "Attach to GPRS")) {
            return true;
        }
        retries--;
        delay(2000);
    }
    return false;
}

// Send AT command and check response
bool sendATCommand(String cmd, int delayTime, String description) {
    while (SIM900.available()) SIM900.read(); // Clear input buffer
    Serial.print("[AT] ");
    Serial.println(description);
    Serial.println(">> " + cmd);
    SIM900.println(cmd);
    delay(100);  // Allow module to process command

    unsigned long start = millis();
    String response;
    while (millis() - start < delayTime) {
        while (SIM900.available()) {
            char c = SIM900.read();
            response += c;
            if (response.endsWith("OK\r\n")) {
                Serial.println("<< " + response);
                return true;
            } else if (response.endsWith("ERROR\r\n")) {
                Serial.println("<< " + response);
                return false;
            }
        }
    }
    Serial.println("<< " + response);
    Serial.println("!! Timeout or no OK received");
    return false;
}

// Connect to the server via TCP
void connectToServer() {
    String cmd = "AT+CIPSTART=\"TCP\",\"" + String(SERVER) + "\",80";
    SIM900.println(cmd);
    Serial.println(">> " + cmd);

    unsigned long start = millis();
    String response;
    while (millis() - start < 10000) {
        while (SIM900.available()) {
            char c = SIM900.read();
            response += c;
            if (response.indexOf("CONNECT OK") != -1 || response.indexOf("ALREADY CONNECT") != -1) {
                Serial.println("<< Connected to server");
                return;
            } else if (response.indexOf("CONNECT FAIL") != -1 || response.indexOf("ERROR") != -1) {
                Serial.println("<< CONNECT FAILED");
                return;
            }
        }
    }
    Serial.println("!! Timeout waiting for CONNECT response");
}

// Send sensor data to the server
void sendDataOnline() {
    Serial.println("\n=== Data Transmission ===");
    String jsonPayload = "{\"temperature\":" + String(temperature, 1)
                       + ",\"tds\":" + String(tdsValue, 0)
                       + ",\"turbidity\":" + String(turbidityVoltage, 2)
                       + ",\"ph\":" + String(phValue, 2) + "}";
    Serial.println("Payload: " + jsonPayload);

    connectToServer();

    String httpRequest = "POST " + String(PATH) + " HTTP/1.1\r\n"
                       + "Host: " + String(SERVER) + "\r\n"
                       + "Content-Type: application/json\r\n"
                       + "Content-Length: " + String(jsonPayload.length()) + "\r\n"
                       + "Connection: close\r\n\r\n"
                       + jsonPayload;
    Serial.println("HTTP Request length: " + String(httpRequest.length()));

    String cmd = "AT+CIPSEND=" + String(httpRequest.length());
    SIM900.println(cmd);
    Serial.println(">> " + cmd);

    unsigned long start = millis();
    while (millis() - start < 10000) {
        if (SIM900.find("> ")) {
            Serial.println("<< Received '>' prompt");
            break;
        }
    }
    if (millis() - start >= 10000) {
        Serial.println("!! Timeout waiting for '>'");
        return;
    }

    SIM900.print(httpRequest);
    Serial.println(">> Sent HTTP request");

    start = millis();
    String response;
    while (millis() - start < 10000) {
        while (SIM900.available()) {
            char c = SIM900.read();
            response += c;
            if (response.indexOf("SEND OK") != -1) {
                Serial.println("<< SEND OK");
                break;
            } else if (response.indexOf("SEND FAIL") != -1 || response.indexOf("ERROR") != -1) {
                Serial.println("<< SEND FAILED");
                break;
            }
        }
        if (response.indexOf("SEND OK") != -1) break;
    }
    if (response.indexOf("SEND OK") == -1) {
        Serial.println("!! Did not receive SEND OK");
    }

    sendATCommand("AT+CIPCLOSE", 3000, "Close Connection");
}

// Read pH sensor data
void readSensorData() {
    for (int i = 0; i < 10; i++) {
        buf[i] = analogRead(SensorPin);
        delay(10);
    }
}

// Sort pH sensor readings
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

// Calculate pH value
float calculatePH() {
    avgValue = 0;
    for (int i = 2; i < 8; i++) {
        avgValue += buf[i];
    }
    float phValueTemp = (float)avgValue * 5.0 / 1024 / 6;
    return 3.5 * phValueTemp;  // Note: May need calibration
}