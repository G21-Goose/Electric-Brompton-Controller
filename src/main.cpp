#include <Arduino.h>
#include "BromptonController.h"
#include "led_control.h"
#include "config.h"

// Global controller instances (constructed in setup)
BromptonController* gBC = nullptr;
// LED/button pins and frame number are centralized in src/config.h
LEDController gLED(LED_RED_PIN, LED_GREEN_PIN, BUTTON_PIN);
int gLastLevel = -1;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Brompton BLE client starting...");

  // Use configured frame number
  String frame = String(FRAME_NUMBER);
  gBC = new BromptonController(frame.c_str());
  gBC->begin();

  gLED.begin();

  Serial.println("Connecting to bike (will retry)...");
  if (!gBC->connect(1000, 10000, 1000, 0)) {
    Serial.println("Failed to connect after retries.");
    return;
  }

  gBC->setLights(true);
  delay(500);
  gBC->setLights(false);
}

void loop() {
  static int lastLevel = -1;
  static unsigned long lastCheck = 0;
  if (gBC && millis() - lastCheck >= 1000) {
    lastCheck = millis();
    int32_t a = gBC->getAssistance();
    if (a != INT32_MIN && (int)a != lastLevel) {
      lastLevel = (int)a;
      gLED.setLevel(lastLevel);
      gLastLevel = lastLevel;
    }
  }
  // handle button press to cycle assistance
  if (gLED.pollButton()) {
    int next = 0;
    if (gLastLevel >= 0) next = (gLastLevel + 1) % 4;
    else {
      int32_t a = (gBC ? gBC->getAssistance() : INT32_MIN);
      next = (a == INT32_MIN) ? 1 : ((int)a + 1) % 4;
    }
    Serial.printf("Button pressed - setting assistance to %d\n", next);
    if (gBC && gBC->setAssistance(next)) {
      gLED.setLevel(next);
      gLastLevel = next;
    }
  }
  // delay(20);
}
