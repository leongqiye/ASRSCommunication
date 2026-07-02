#include <Arduino.h>
#include <WiFi.h>
#include <ASRS_Comm_UART.h>
#include <ASRS_Comm_ESPNow.h>
#include <ASRS_Slave.h>
#include <ASRS_Motion.h>
#include <session/ASRS_ESPNow_SlaveSession.h>

static constexpr uint32_t ASRS_UART_BAUD = 115200;
static constexpr uint8_t ASRS_ESPNOW_CHANNEL = 1;
static constexpr uint8_t ASRS_DIP_PIN = 2;
static constexpr uint8_t ASRS_DIP_ESPNOW_LEVEL = HIGH;
static constexpr int8_t ASRS_UART_TX_PIN = 17;
static constexpr int8_t ASRS_UART_RX_PIN = 18;
static constexpr int32_t ASRS_HOME_X_POSITION_MM = 0;
static constexpr int32_t ASRS_HOME_Z_POSITION_MM = 0;

// Used only to monitor wrong-mode ESP-NOW traffic while UART is selected.
static uint8_t masterPeerAddress[6] = {0xE0, 0x72, 0xA1, 0xF4, 0xA0, 0x74};

ASRS_Comm_UART uartCommunication(Serial1);
ASRS_Comm_ESPNow espNowCommunication;
ASRS_ESPNow_SlaveSession espNowSession(espNowCommunication);
ASRS_Comm_Base *selectedCommunication = nullptr;
ASRS_Slave *slave = nullptr;

static bool communicationReady = false;
static bool useEspNow = false;

static void updateSelectedCommunication() {
  if (slave == nullptr) {
    return;
  }

  if (useEspNow) {
    espNowSession.update();

    if (!espNowSession.connected()) {
      return;
    }

    if (espNowCommunication.available()) {
      espNowSession.recordPeerActivity();
    }
  }

  slave->update();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(ASRS_DIP_PIN, INPUT_PULLUP);
  useEspNow =
      digitalRead(ASRS_DIP_PIN) == ASRS_DIP_ESPNOW_LEVEL;

  uartCommunication.begin(ASRS_UART_BAUD, ASRS_UART_RX_PIN, ASRS_UART_TX_PIN);

  bool espNowReady = false;

  if (useEspNow) {
    Serial.println("ASRS tower board configured as ESP-NOW slave.");

    if (!espNowSession.begin(
            true,
            ASRS_ESPNOW_CHANNEL,
            false,
            &Serial)) {
      Serial.print("ESP-NOW initialisation failed: ");
      Serial.println(asrsErrorName(espNowSession.lastError()));
      return;
    }

    espNowReady = true;
    selectedCommunication = &espNowCommunication;
  } else {
    Serial.println("ASRS tower board configured as UART slave.");
    selectedCommunication = &uartCommunication;

    // A fixed peer is initialized only for wrong-mode reporting while UART is selected.
    espNowReady =
        espNowCommunication.begin(
            masterPeerAddress,
            ASRS_ESPNOW_CHANNEL);
  }

  static ASRS_Slave slaveObject(*selectedCommunication);
  slave = &slaveObject;

  if (useEspNow) {
    slave->setWrongModeCommunication(uartCommunication);
  } else if (espNowReady) {
    slave->setWrongModeCommunication(espNowCommunication);
  }

  slave->setOperationStatus(ASRS_STATUS_IDLE);
  slave->setHomePosition(ASRS_HOME_X_POSITION_MM, ASRS_HOME_Z_POSITION_MM);
  slave->enableMotionControl(true);

  if (slave->beginTofCoordinateSensors()) {
    if (!slave->measureCoordinatesFromTof(true)) {
      Serial.println("Initial VL53L1X coordinate measurement failed.");
    }
  } else {
    Serial.println("VL53L1X ToF sensor initialisation failed.");
  }

  Serial.print("Slave station MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println(
      "GPIO2 LOW selects UART; GPIO2 HIGH selects ESP-NOW.");
  Serial.println("UART TX: GPIO17, UART RX: GPIO18.");
  Serial.println("Slave motion-control firmware is active.");
  Serial.println("Travel commands are interpreted as absolute X/Z target coordinates in integer millimetres.");
  Serial.println("Stepper control uses GPIO4/GPIO5 for X and GPIO6/GPIO7 for Z.");
  Serial.println("Limit switches use X-min GPIO38, X-max GPIO39, Z-min GPIO40, Z-max GPIO41 with active-LOW INPUT_PULLUP logic.");
  Serial.println("Homing moves X toward X-min GPIO38 and Z toward Z-min GPIO40.");
  if (slave->tofCoordinateSensorsReady()) {
    Serial.println("VL53L1X tof1 uses XSHUT GPIO10 and measures toward X-min.");
    Serial.println("VL53L1X tof2 uses XSHUT GPIO11 and measures toward Z-min.");
    Serial.println("VL53L1X tof3 uses XSHUT GPIO12 as a diagnostic sensor.");
    Serial.println("The slave measures tof1/tof2 during setup and before replying to coordinate requests.");
  }
  if (useEspNow) {
    Serial.println("ESP-NOW mode uses dynamic pairing.");
  } else if (espNowReady) {
    Serial.println(
        "ESP-NOW wrong-mode monitor peer: E0:72:A1:F4:A0:74.");
  }

  communicationReady = true;
}

void loop() {
  if (!communicationReady) {
    delay(1000);
    return;
  }

  updateSelectedCommunication();
}
