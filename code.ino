void setup() {
  Serial.begin(9600);
  // Baud rate: 9600
}

void loop() {
  int sensorValue = analogRead(A0);  // Read the input on analog pin 0

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

  delay(500); // Delay to avoid excessive serial output
}
