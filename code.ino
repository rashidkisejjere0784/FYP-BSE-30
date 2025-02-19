#include <OneWire.h> 
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(9600);
  // Baud rate: 9600

  sensors.begin(); // begin temperature sensor
}

void loop() {
  int sensorValue = analogRead(A0);  // Read the input on analog pin 0
  sensors.requestTemperatures();

  Serial.print("Temperature is: "); 
  Serial.print(sensors.getTempCByIndex(1));
  

  // Convert the analog reading (which goes from 0 to 1023) to a voltage (0 to 5V)
  float voltage = sensorValue * (5.0 / 1024.0);

  Serial.println(voltage); // Print the voltage value

  // Check turbidity levels
  if (voltage <= 2.5) {
    Serial.println("Turbidity > 3000");
  } else if (voltage > 2.5 && voltage <= 3) {
    Serial.println("Turbidity b/w 3000 - 2700");
  } else if (voltage > 3 && voltage <= 3.5) {
    Serial.println("Turbidity b/w 2700 - 2000");
  } else if (voltage > 3.5 && voltage <= 4.25) {
    Serial.println("Turbidity b/w 2000 - 750");
  } else if (voltage > 4.25) {
    Serial.println("No-Turbidity");
  } else {
    Serial.println("Error in reading");
  }

  delay(1500); // Delay to avoid excessive serial output
}
