# Electric Brompton Assistance Wireless Controller

This PlatformIO project is to allow an ESP32 to connect to a Brompton Electric Mk1 over BLE, pairs using a pin derived from the bike frame number, and provides helpers to read/write bike state (assistance, battery, lights, odometer, speed, torque, cadence, etc). The primary purpose of this is to control the assistance using a button that can be placed on the handlebars with a Red/Green LED that can then show the assistance (No Assistance - Off, Level 1 - Green, Level 2 - Yellow by mixing Red/Green, Level 3 - Red). After pairing, the lights will turn on and off to show it is ready. If you change the assistance on the battery, the indicator light connected to the ESP32 will update.

Key files (what exists today)
- `platformio.ini` - PlatformIO build configuration and library deps (NimBLE-Arduino).
- `src/config.h` - central configuration: `FRAME_NUMBER`, `LED_RED_PIN`, `LED_GREEN_PIN`, `BUTTON_PIN`.
- `src/main.cpp` - application entry: constructs `BromptonController` and `LEDController`, connects to the bike, toggles lights briefly on connect, polls assistance and responds to the button.
- `src/BromptonController.h/.cpp` - BLE client abstraction. Implements:
  - `generate_passkey()` - derives the 6-digit passkey from `FRAME_NUMBER` (see algorithm below).
  - `begin()` - initializes NimBLE, configures security, and sets the passkey.
  - `connect()`/`disconnect()`/`isConnected()` and characteristic getters/setters such as `getAssistance()`, `setAssistance()`, `getBattery()`, `setLights()`, `getOdometer()`, etc.
- `src/led_control.h/.cpp` - LED + button code using LEDC. Exposes `LEDController::setLevel(int)` (0..3) and `LEDController::writeBoth(redDuty, greenDuty)`.

Build & flash
1. Edit `src/config.h` and set `FRAME_NUMBER` and pins.
2. Build and upload:

```bash
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -e esp32dev --baud 115200
```

BLE pairing and passkey - exact algorithm used
The project implements the passkey algorithm directly in `BromptonController::generate_passkey()`; this is how the 6-digit PIN is produced from the bike frame:

1. Create a 16-byte input buffer and copy the ASCII bytes of the frame number into it. Any remaining bytes are zero.
2. Use a constant 16-byte AES-128 key where key[i] = i+1 (i.e., 0x01,0x02,...,0x10).
3. Encrypt the 16-byte input with AES-128 in ECB mode (mbedTLS AES ECB is used).
4. Take the encrypted output's first two bytes and form a 16-bit unsigned value: v = (out[0] << 8) | out[1].
 5. Format `v` as a zero-padded 6-digit decimal string (e.g. "000123"); that string is used as the BLE passkey.

In code: the passkey is set via `NimBLEDevice::setSecurityPasskey(passkey)` inside `BromptonController::begin()`.

Example
- Frame number `1234567` -> AES-ECB output first two bytes yield value `3624` -> passkey `003624`.

Security defaults in the code (edit in `BromptonController::begin()` if needed)
- `NimBLEDevice::deleteAllBonds()` is called by default (clears previous bonds stored on the ESP32).
- `setSecurityAuth()` is called with bonding, MITM and Secure Connections enabled.
- IO capability is set to `BLE_HS_IO_KEYBOARD_ONLY` so the controller provides the passkey to the peer.

LED and button details
- `LEDController` initializes LEDC PWM on the pins defined in `src/config.h`. PWM resolution defaults to 8 bits (0..255) and frequency is set in the code.
- Use `gLED.writeBoth(redDuty, greenDuty)` to set both channels directly (values 0..ledcMaxDuty). Assistance-level default mapping is in `LEDController::applyStatic()`.

Runtime behavior summary
- On startup the controller computes the passkey from `FRAME_NUMBER`, configures NimBLE security, then scans for advertisements where device name == "Brompton_Elec" and attempts to connect.
- Once connected, it discovers and caches the main Brompton service and characteristic handles it needs (assistance, lights, battery). Other characteristics are fetched on demand.
- The main loop polls `getAssistance()` every second and updates LEDs. Pressing the button cycles assistance 0..3 and writes the new level to the bike.

Troubleshooting
- If pairing fails: confirm `FRAME_NUMBER` is correct and the bike is in pairing mode. If you suspect stale bonding state, try clearing bonds on the bike and/or the ESP32.
- If one LED is too bright: reduce that channel's duty in `LEDController::applyStatic()` or call `gLED.writeBoth()` from code to tune.

Notes
- `src/config.h` holds data that effectively determines the pairing PIN - treat it as sensitive if you do not want to expose your passkey.

