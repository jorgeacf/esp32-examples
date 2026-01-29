#include <Arduino.h>

#define LED_PIN 2

void setup() {
  pinMode(LED_PIN, OUTPUT);
  ledcAttach(LED_PIN, 5000, 8);  // Auto-assigns channel, 5 kHz, 8-bit resolution (0-255)
  
  Serial.begin(115200);
  Serial.println("ESP32 PWM Fade (Arduino 3.x) started!");
}

void loop() {
  // Fade from OFF → full ON (active-low LED: duty 255 = off, 0 = full on)
  for (int duty = 255; duty >= 0; --duty) {
    ledcWrite(LED_PIN, duty);
    delay(10);
  }

  // ----- 3 quick blinks -----
  for (int i = 0; i < 3; i++) {
    ledcWrite(LED_PIN, 255);   // LED OFF (duty 255 = off)
    delay(150);
    ledcWrite(LED_PIN, 0);     // LED full ON (duty 0 = on)
    delay(150);
  }

  // Small pause after the blinks so they are clearly separated from the fade
  delay(400);

  // Fade from full ON → OFF
  for (int duty = 0; duty <= 255; ++duty) {
    ledcWrite(LED_PIN, duty);
    delay(10);
  }

  // Optional: small delay at the end before the next cycle starts
  delay(500);
}