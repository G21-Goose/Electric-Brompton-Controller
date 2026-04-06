#include "BromptonController.h"
#include "mbedtls/aes.h"

// Hardcode service and characteristic UUIDs used by Brompton
static const char* BROMPTON_SERVICE = "105c6761-74bf-4ffe-94ea-f8ba79f20600";
static const char* BROMPTON_ASSIST_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20612";

// Additional Brompton characteristic UUIDs (from Python mapping)
static const char* BROMPTON_ODOMETER_SVC = "95f99abc-871d-4167-a032-7283d612bd00";
static const char* BROMPTON_ODOMETER_CHAR = "95f99abc-871d-4167-a032-7283d612bd01";
static const char* BROMPTON_SPEED_CHAR = "95f99abc-871d-4167-a032-7283d612bd02";
static const char* BROMPTON_TORQUE_CHAR = "95f99abc-871d-4167-a032-7283d612bd03";
static const char* BROMPTON_CADENCE_CHAR = "95f99abc-871d-4167-a032-7283d612bd04";

static const char* BROMPTON_RIDE_DISTANCE_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20603";
static const char* BROMPTON_POWER_ON_TIME_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20602";
static const char* BROMPTON_AVG_RIDE_DISTANCE_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20604";
static const char* BROMPTON_MOTOR_ON_TIME_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20605";
static const char* BROMPTON_LIGHTS_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20611";
static const char* BROMPTON_BATTERY_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20615";
static const char* BROMPTON_BATTERY_CYCLES_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20616";
static const char* BROMPTON_SOH_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20617";
static const char* BROMPTON_MOTOR_DISTANCE_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f20619";
static const char* BROMPTON_AKKU_VOLTAGE_CHAR = "105c6761-74bf-4ffe-94ea-f8ba79f2061a";

BromptonController::BromptonController(const char* frameNumber)
  : frame(String(frameNumber)), svcUUID(NimBLEUUID(BROMPTON_SERVICE)), assistUUID(NimBLEUUID(BROMPTON_ASSIST_CHAR)),
    batteryUUID(NimBLEUUID(BROMPTON_BATTERY_CHAR)), lightsUUID(NimBLEUUID(BROMPTON_LIGHTS_CHAR)) {
  String pass = generate_passkey(frame.c_str());
  passkey = pass.toInt();
}

String BromptonController::generate_passkey(const char* frame) {
  uint8_t key[16];
  for (int i = 0; i < 16; ++i) key[i] = i + 1;
  uint8_t input[16];
  size_t flen = strlen(frame);
  if (flen > 16) flen = 16;
  memset(input, 0, 16);
  memcpy(input, frame, flen);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 128);
  uint8_t out[16];
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input, out);
  mbedtls_aes_free(&aes);

  uint16_t v = ((uint16_t)out[0] << 8) | out[1];
  char buf[8];
  snprintf(buf, sizeof(buf), "%06u", (unsigned)v);
  return String(buf);
}

void BromptonController::begin() {
  // Defaults chosen for Brompton pairing; tweak here if needed
  const bool CLEAR_BONDS = true;
  const bool SEC_BONDING = true;
  const bool SEC_MITM = true;
  const bool SEC_SC = true;
  const int SEC_IO_CAP = BLE_HS_IO_KEYBOARD_ONLY;

  NimBLEDevice::init("");
  if (CLEAR_BONDS) NimBLEDevice::deleteAllBonds();
  NimBLEDevice::setSecurityAuth(SEC_BONDING, SEC_MITM, SEC_SC);
  NimBLEDevice::setSecurityIOCap(SEC_IO_CAP);
  NimBLEDevice::setSecurityPasskey(passkey);
}

bool BromptonController::connect(int scanMs, int connectTimeoutMs, int retryDelayMs, int maxRetries) {
  int attempts = 0;
  while (true) {
    attempts++;
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->start(scanMs);
    delay(scanMs + 50);
    NimBLEScanResults results = pScan->getResults();
    NimBLEAddress addr;
    bool found = false;
    for (int i = 0; i < results.getCount(); ++i) {
      const NimBLEAdvertisedDevice* adv = results.getDevice(i);
      if (adv && adv->haveName() && adv->getName() == "Brompton_Elec") {
        addr = adv->getAddress();
        found = true;
        break;
      }
    }
    if (!found) {
      if (maxRetries > 0 && attempts >= maxRetries) return false;
      delay(retryDelayMs);
      continue;
    }

    client = NimBLEDevice::createClient();
    client->setClientCallbacks(new ClientCallbacks(this), true);
    client->setConnectTimeout(connectTimeoutMs);
    if (client->connect(addr)) {
      NimBLERemoteService *svc = client->getService(svcUUID);
      if (!svc) {
        NimBLEDevice::deleteClient(client);
        client = nullptr;
        return false;
      }
      assistCh = svc->getCharacteristic(assistUUID);
      batteryCh = svc->getCharacteristic(batteryUUID);
      lightsCh = svc->getCharacteristic(lightsUUID);
      // Also attempt to attach other known characteristics if present
      // odometer and related are on a different service; fetch by UUID when requested
      return true;
    } else {
      NimBLEDevice::deleteClient(client);
      client = nullptr;
      if (maxRetries > 0 && attempts >= maxRetries) return false;
      delay(retryDelayMs);
    }
  }
}

void BromptonController::disconnect() {
  if (client) {
    if (client->isConnected()) client->disconnect();
    NimBLEDevice::deleteClient(client);
    client = nullptr;
  }
}

bool BromptonController::isConnected() {
  return client && client->isConnected();
}

bool BromptonController::setAssistance(int32_t level) {
  if (!client || !client->isConnected() || !assistCh) return false;
  uint8_t buf[4];
  buf[0] = level & 0xFF;
  buf[1] = (level >> 8) & 0xFF;
  buf[2] = (level >> 16) & 0xFF;
  buf[3] = (level >> 24) & 0xFF;
  assistCh->writeValue(buf, 4, true);
  return true;
}

int32_t BromptonController::getAssistance() {
  if (!client || !client->isConnected() || !assistCh) return INT32_MIN;
  std::string val = assistCh->readValue();
  if (val.size() < 4) return INT32_MIN;
  int32_t rv = (uint8_t)val[0] | ((uint8_t)val[1] << 8) | ((uint8_t)val[2] << 16) | ((uint8_t)val[3] << 24);
  return rv;
}

float BromptonController::getBattery() {
  if (!client || !client->isConnected()) return std::numeric_limits<float>::quiet_NaN();
  if (!batteryCh) {
    // try to resolve characteristic from service
    NimBLERemoteService *svc = client->getService(svcUUID);
    if (svc) batteryCh = svc->getCharacteristic(NimBLEUUID(BROMPTON_BATTERY_CHAR));
  }
  if (!batteryCh) return std::numeric_limits<float>::quiet_NaN();
  std::string val = batteryCh->readValue();
  if (val.size() < 4) return std::numeric_limits<float>::quiet_NaN();
  float f;
  memcpy(&f, val.data(), 4);
  return f;
}

bool BromptonController::setLights(int val) {
  if (!client || !client->isConnected()) return false;
  if (!lightsCh) {
    NimBLERemoteService *svc = client->getService(svcUUID);
    if (svc) lightsCh = svc->getCharacteristic(NimBLEUUID(BROMPTON_LIGHTS_CHAR));
  }
  if (!lightsCh) return false;
  uint8_t buf[4];
  buf[0] = val & 0xFF;
  buf[1] = (val >> 8) & 0xFF;
  buf[2] = (val >> 16) & 0xFF;
  buf[3] = (val >> 24) & 0xFF;
  lightsCh->writeValue(buf, 4, true);
  return true;
}

float BromptonController::getOdometer() {
  if (!client || !client->isConnected()) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteService *svc = client->getService(NimBLEUUID(BROMPTON_ODOMETER_SVC));
  if (!svc) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_ODOMETER_CHAR));
  if (!ch) return std::numeric_limits<float>::quiet_NaN();
  std::string v = ch->readValue();
  if (v.size() < 4) return std::numeric_limits<float>::quiet_NaN();
  float f; memcpy(&f, v.data(), 4); return f;
}

float BromptonController::getSpeed() {
  if (!client || !client->isConnected()) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteService *svc = client->getService(NimBLEUUID(BROMPTON_ODOMETER_SVC));
  if (!svc) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_SPEED_CHAR));
  if (!ch) return std::numeric_limits<float>::quiet_NaN();
  std::string v = ch->readValue(); if (v.size() < 4) return std::numeric_limits<float>::quiet_NaN(); float f; memcpy(&f, v.data(), 4); return f;
}

float BromptonController::getTorque() {
  if (!client || !client->isConnected()) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteService *svc = client->getService(NimBLEUUID(BROMPTON_ODOMETER_SVC));
  if (!svc) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_TORQUE_CHAR));
  if (!ch) return std::numeric_limits<float>::quiet_NaN();
  std::string v = ch->readValue(); if (v.size() < 4) return std::numeric_limits<float>::quiet_NaN(); float f; memcpy(&f, v.data(), 4); return f;
}

float BromptonController::getCadence() {
  if (!client || !client->isConnected()) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteService *svc = client->getService(NimBLEUUID(BROMPTON_ODOMETER_SVC));
  if (!svc) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_CADENCE_CHAR));
  if (!ch) return std::numeric_limits<float>::quiet_NaN();
  std::string v = ch->readValue(); if (v.size() < 4) return std::numeric_limits<float>::quiet_NaN(); float f; memcpy(&f, v.data(), 4); return f;
}

std::vector<float> BromptonController::getRideDistance() {
  std::vector<float> out;
  if (!client || !client->isConnected()) return out;
  NimBLERemoteService *svc = client->getService(svcUUID);
  if (!svc) return out;
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_RIDE_DISTANCE_CHAR));
  if (!ch) return out;
  std::string v = ch->readValue();
  size_t cnt = v.size() / 4;
  out.reserve(cnt);
  for (size_t i = 0; i < cnt; ++i) {
    float f; memcpy(&f, v.data() + i*4, 4); out.push_back(f);
  }
  return out;
}

int32_t BromptonController::getPowerOnTime() {
  if (!client || !client->isConnected()) return INT32_MIN;
  NimBLERemoteService *svc = client->getService(svcUUID);
  if (!svc) return INT32_MIN;
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_POWER_ON_TIME_CHAR));
  if (!ch) return INT32_MIN;
  std::string v = ch->readValue(); if (v.size() < 4) return INT32_MIN; int32_t r; memcpy(&r, v.data(), 4); return r;
}

float BromptonController::getAverageRideDistance() {
  if (!client || !client->isConnected()) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteService *svc = client->getService(svcUUID);
  if (!svc) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_AVG_RIDE_DISTANCE_CHAR));
  if (!ch) return std::numeric_limits<float>::quiet_NaN();
  std::string v = ch->readValue(); if (v.size() < 4) return std::numeric_limits<float>::quiet_NaN(); float f; memcpy(&f, v.data(), 4); return f;
}

int32_t BromptonController::getMotorOnTime() {
  if (!client || !client->isConnected()) return INT32_MIN;
  NimBLERemoteService *svc = client->getService(svcUUID);
  if (!svc) return INT32_MIN;
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_MOTOR_ON_TIME_CHAR));
  if (!ch) return INT32_MIN;
  std::string v = ch->readValue(); if (v.size() < 4) return INT32_MIN; int32_t r; memcpy(&r, v.data(), 4); return r;
}

int32_t BromptonController::getLights() {
  if (!client || !client->isConnected()) return INT32_MIN;
  if (!lightsCh) {
    NimBLERemoteService *svc = client->getService(svcUUID);
    if (svc) lightsCh = svc->getCharacteristic(NimBLEUUID(BROMPTON_LIGHTS_CHAR));
  }
  if (!lightsCh) return INT32_MIN;
  std::string v = lightsCh->readValue(); if (v.size() < 4) return INT32_MIN; int32_t r; memcpy(&r, v.data(), 4); return r;
}

int32_t BromptonController::getBatteryCycles() {
  if (!client || !client->isConnected()) return INT32_MIN;
  NimBLERemoteService *svc = client->getService(svcUUID);
  if (!svc) return INT32_MIN;
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_BATTERY_CYCLES_CHAR));
  if (!ch) return INT32_MIN;
  std::string v = ch->readValue(); if (v.size() < 4) return INT32_MIN; int32_t r; memcpy(&r, v.data(), 4); return r;
}

int32_t BromptonController::getStateOfHealth() {
  if (!client || !client->isConnected()) return INT32_MIN;
  NimBLERemoteService *svc = client->getService(svcUUID);
  if (!svc) return INT32_MIN;
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_SOH_CHAR));
  if (!ch) return INT32_MIN;
  std::string v = ch->readValue(); if (v.size() < 4) return INT32_MIN; int32_t r; memcpy(&r, v.data(), 4); return r;
}

std::vector<float> BromptonController::getMotorDistance() {
  std::vector<float> out;
  if (!client || !client->isConnected()) return out;
  NimBLERemoteService *svc = client->getService(svcUUID);
  if (!svc) return out;
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_MOTOR_DISTANCE_CHAR));
  if (!ch) return out;
  std::string v = ch->readValue(); size_t cnt = v.size()/4; out.reserve(cnt); for (size_t i=0;i<cnt;++i){ float f; memcpy(&f, v.data()+i*4,4); out.push_back(f);} return out;
}

float BromptonController::getAkkuVoltage() {
  if (!client || !client->isConnected()) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteService *svc = client->getService(svcUUID);
  if (!svc) return std::numeric_limits<float>::quiet_NaN();
  NimBLERemoteCharacteristic* ch = svc->getCharacteristic(NimBLEUUID(BROMPTON_AKKU_VOLTAGE_CHAR));
  if (!ch) return std::numeric_limits<float>::quiet_NaN();
  std::string v = ch->readValue(); if (v.size() < 4) return std::numeric_limits<float>::quiet_NaN(); float f; memcpy(&f, v.data(), 4); return f;
}
