#include "led_control.h"
#include "driver/ledc.h"

LEDController::LEDController(int red, int green, int button)
  : redPin(red), greenPin(green), buttonPin(button) {}

void LEDController::begin() {
  // configure PWM (LEDC) for both LED pins
  ledcMaxDuty = (1u << ledcResolution) - 1;
  ledcSetup(ledcTimer, ledcFreq, ledcResolution);
  ledcAttachPin(greenPin, ledcChanGreen);
  ledcAttachPin(redPin, ledcChanRed);
  // start with both off
  writeBoth(0, 0);

  if (buttonPin >= 0) {
    // external pulldown resistor: use INPUT (no internal pull-up)
    pinMode(buttonPin, INPUT);
    lastReadState = digitalRead(buttonPin);
    lastStableState = lastReadState;
    lastBounceTime = millis();
  }
}

void LEDController::setLevel(int lvl) {
  if (lvl < 0) lvl = 0;
  if (lvl > 3) lvl = 3;
  level = lvl;
  // apply static PWM mapping for each level
  applyStatic();
}

void LEDController::applyStatic() {
  switch (level) {
    case 0: // off
      writeBoth(0, 0);
      break;
    case 1: // green
      writeBoth(0, ledcMaxDuty);
      break;
    case 3: // red
      writeBoth(ledcMaxDuty, 0);
      break;
    case 2: // handled in update() to alternate quickly
    default:
      // set both on (yellow-ish); default: green full, red reduced so it's not overpowering
      writeBoth(ledcMaxDuty/4, ledcMaxDuty);
      break;
  }
}

bool LEDController::pollButton() {
  if (buttonPin < 0) return false;
  int reading = digitalRead(buttonPin);
  if (reading != lastReadState) {
    lastBounceTime = millis();
    lastReadState = reading;
  }
  if (millis() - lastBounceTime > debounceMs) {
    if (reading != lastStableState) {
      lastStableState = reading;
      // external pulldown: pressed == HIGH
      if (lastStableState == HIGH) {
        return true;
      }
    }
  }
  return false;
}

void LEDController::writeBoth(uint32_t redDuty, uint32_t greenDuty) {
  uint32_t r = redDuty & ledcMaxDuty;
  uint32_t g = greenDuty & ledcMaxDuty;
  // write both duties atomically
  noInterrupts();
  ledcWrite(ledcChanRed, r);
  ledcWrite(ledcChanGreen, g);
  interrupts();
}
