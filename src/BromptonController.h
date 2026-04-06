#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>
#include <limits>

class BromptonController {
public:
  BromptonController(const char* frameNumber);

  // Initialize BLE + security using hardcoded defaults for Brompton pairing
  void begin();

  bool connect(int scanMs = 5000, int connectTimeoutMs = 10000, int retryDelayMs = 5000, int maxRetries = 0);
  void disconnect();
  bool isConnected();

  bool setAssistance(int32_t level);
  int32_t getAssistance();

  // Brompton-specific getters/setters (UUIDs are hardcoded)
  float getOdometer();
  float getSpeed();
  float getTorque();
  float getCadence();
  std::vector<float> getRideDistance();
  int32_t getPowerOnTime();
  float getAverageRideDistance();
  int32_t getMotorOnTime();
  int32_t getLights();
  bool setLights(int val);
  float getBattery();
  int32_t getBatteryCycles();
  int32_t getStateOfHealth();
  std::vector<float> getMotorDistance();
  float getAkkuVoltage();

private:
  String frame;
  uint32_t passkey = 0;
  NimBLEClient* client = nullptr;
  NimBLERemoteCharacteristic* assistCh = nullptr;
  NimBLERemoteCharacteristic* batteryCh = nullptr;
  NimBLERemoteCharacteristic* lightsCh = nullptr;

  NimBLEUUID svcUUID;
  NimBLEUUID assistUUID;
  NimBLEUUID batteryUUID;
  NimBLEUUID lightsUUID;

  String generate_passkey(const char* frame);

  class ClientCallbacks : public NimBLEClientCallbacks {
  public:
    ClientCallbacks(BromptonController* parent) : parent(parent) {}
    void onPassKeyEntry(NimBLEConnInfo& connInfo) override { NimBLEDevice::injectPassKey(connInfo, parent->passkey); }
    uint32_t onPassKeyDisplay(NimBLEConnInfo& connInfo) override { return parent->passkey; }
    void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pin) override { NimBLEDevice::injectConfirmPasskey(connInfo, true); }
    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {}
  private:
    BromptonController* parent;
  };
};
