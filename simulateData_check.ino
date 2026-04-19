#include <math.h>

#define THRESHOLD 650   // Leak detection threshold

void setup() {
  Serial.begin(115200);
}

void loop() {

  static int timeCounter = 0;
  float value;

  // -------- NORMAL CONDITION (first 10 seconds) --------
  if (timeCounter < 100) {
    value = 500 + random(-20, 20);  // small variation
  }

  // -------- LEAK CONDITION (after 10 seconds) --------
  else {
    value = 800 + random(-50, 50);  // high abnormal values
  }

  // -------- PRINT FOR GRAPH --------
  Serial.print(value);          // Sensor value
  Serial.print(",");            
  Serial.print(THRESHOLD);      // Threshold line
  Serial.print(",");

  // Leak detection flag (for visualization)
  if (value > THRESHOLD) {
    Serial.println(900);        // Leak indicator line
  } else {
    Serial.println(0);
  }

  timeCounter++;
  delay(100);
}