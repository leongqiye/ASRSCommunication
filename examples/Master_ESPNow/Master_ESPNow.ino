#include <Arduino.h>
#include <ASRSCommunication.h>

#if !defined(ESP32)
#error "The ESP-NOW example requires an ESP32-compatible board."
#endif

static constexpr uint8_t ASRS_ESPNOW_CHANNEL = 1;
static constexpr uint32_t REQUEST_INTERVAL_MS = 2000;

uint8_t slavePeerAddress[6] = {0xE0, 0x72, 0xA1, 0xF4, 0xA3, 0x7C};

ASRS_Comm_ESPNow espNowCommunication;
ASRS_Master master(espNowCommunication);

static uint32_t lastRequestTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!espNowCommunication.begin(slavePeerAddress, ASRS_ESPNOW_CHANNEL)) {
    Serial.print("ESP-NOW initialisation failed: ");
    Serial.println(asrsErrorName(espNowCommunication.lastError()));
    return;
  }

  Serial.println("ASRS ESP-NOW master example started.");
}

void loop() {
  const uint32_t now = millis();
  if (now - lastRequestTime >= REQUEST_INTERVAL_MS) {
    lastRequestTime = now;

    ASRS_Coordinates coordinates;
    if (master.requestCoordinates(coordinates)) {
      Serial.print("Coordinate response: X = ");
      Serial.print(coordinates.x);
      Serial.print(", Z = ");
      Serial.println(coordinates.z);
    } else {
      Serial.print("Coordinate request failed: ");
      Serial.println(asrsErrorName(master.lastError()));
    }
  }

  ASRS_OperationStatus status;
  if (master.readOperationStatus(status)) {
    Serial.print("Operation status: ");
    Serial.println(asrsStatusName(status.status));
  }
}
