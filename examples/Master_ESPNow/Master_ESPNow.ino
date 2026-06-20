#include <Arduino.h>
#include <ASRSCommunication.h>

#if !defined(ESP32)
#error "The ESP-NOW example requires an ESP32-compatible board."
#endif

static constexpr uint8_t ASRS_ESPNOW_CHANNEL = 1;
static constexpr uint32_t REQUEST_INTERVAL_MS = 2000;
static constexpr uint32_t INITIAL_PAIRING_TIMEOUT_MS = 10000;

ASRS_Comm_ESPNow espNowCommunication;
ASRS_ESPNow_MasterSession espNowSession(espNowCommunication);
ASRS_Master master(espNowCommunication);

static uint32_t lastRequestTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!espNowSession.begin(ASRS_ESPNOW_CHANNEL, INITIAL_PAIRING_TIMEOUT_MS, &Serial)) {
    Serial.print("ESP-NOW master session initialisation failed: ");
    Serial.println(asrsErrorName(espNowSession.lastError()));
    return;
  }

  Serial.println("ASRS ESP-NOW master example started with dynamic pairing.");
}

void loop() {
  espNowSession.update();
  if (!espNowSession.connected()) {
    delay(50);
    return;
  }

  const uint32_t now = millis();
  if (now - lastRequestTime >= REQUEST_INTERVAL_MS) {
    lastRequestTime = now;

    ASRS_Coordinates coordinates;
    if (master.requestCoordinates(coordinates)) {
      espNowSession.recordPeerActivity();
      Serial.print("Coordinate response: X = ");
      Serial.print(coordinates.x);
      Serial.print(", Z = ");
      Serial.println(coordinates.z);
    } else {
      Serial.print("Coordinate request failed: ");
      Serial.println(asrsErrorName(master.lastError()));
    }
  }
  //Add send travel command example here.
  ASRS_OperationStatus status;
  if (master.readOperationStatus(status)) {
    espNowSession.recordPeerActivity();
    Serial.print("Operation status: ");
    Serial.println(asrsStatusName(status.status));
  }
}
