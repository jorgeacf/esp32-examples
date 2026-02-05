#include <Arduino.h>
#include <ESP32Servo.h>

Servo myservo;
const int servoPin  = 13;       // Your servo signal pin
const int buttonPin = 0;        // BOOT button (GPIO 0)

// Debounce variables
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;  // 50 ms typical debounce

// Track last stable button state to detect presses (falling edge)
int lastButtonState = HIGH;     // assume released at start

void setup() {
  Serial.begin(115200);
  Serial.println("Servo toggle by BOOT button - started");

  myservo.attach(servoPin, 500, 2500);

  pinMode(buttonPin, INPUT_PULLUP);

  // Start at 0° (button not pressed)
  myservo.write(0);
  Serial.println("Servo at 0° (default - button released)");
}

void loop() {
  int reading = digitalRead(buttonPin);

  if (reading == LOW) {
    // Falling edge detected → button was just pressed
    Serial.println("Button PRESSED → moving to 180°");
    myservo.write(180);
  }

  // When button is NOT pressed (HIGH), keep servo at 0°
  if (reading == HIGH) {
    myservo.write(0);
  }

  delay(10);  // small loop delay for stability & low CPU use
}