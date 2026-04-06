#pragma once
#include <Arduino.h>

class LEDController {
public:
  // pins: red, green, optional button (use -1 if not used)
  // PWM is always used (LEDC)
  LEDController(int redPin, int greenPin, int buttonPin);
  void begin();
  // set assistance level: 0..3
  void setLevel(int level);
  // poll the debounced button; returns true once per press (rising edge)
  bool pollButton();

private:
  int redPin;
  int greenPin;
  int buttonPin;
  int level = 0;
  // removed alternating/flashing state - replaced with simultaneous PWM control
  // debouncing state
  int lastReadState = LOW;
  int lastStableState = LOW;
  unsigned long lastBounceTime = 0;
  unsigned long debounceMs = 50;
  // PWM (LEDC) members
  int ledcTimer = 0;
  int ledcFreq = 2000;
  int ledcResolution = 8; // bits
  int ledcChanGreen = 0;
  int ledcChanRed = 1;
  uint32_t ledcMaxDuty = 255;
  // write independent PWM duties for red/green (0..ledcMaxDuty)
  void writeBoth(uint32_t redDuty, uint32_t greenDuty);
  // allow user to set both PWM channels directly
  void setBothPWM(uint32_t redDuty, uint32_t greenDuty);
  void applyStatic();
};
