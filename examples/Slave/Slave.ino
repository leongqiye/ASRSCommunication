#include <Arduino.h>
#include <ASRSCommunication.h>

static constexpr uint32_t ASRS_UART_BAUD = 115200;

#if defined(ESP32)
static constexpr uint8_t ASRS_ESPNOW_CHANNEL = 1;
static constexpr uint8_t ASRS_DIP_PIN = 2;
static constexpr uint8_t ASRS_DIP_ESPNOW_LEVEL = HIGH;
static constexpr int8_t ASRS_UART_RX_PIN = 18;
static constexpr int8_t ASRS_UART_TX_PIN = 17;

ASRS_Comm_ESPNow espNowCommunication;
ASRS_ESPNow_SlaveSession espNowSession(espNowCommunication);
ASRS_Slave espNowSlave(espNowCommunication);
#else
static constexpr int8_t ASRS_UART_RX_PIN = -1;
static constexpr int8_t ASRS_UART_TX_PIN = -1;
#endif

ASRS_Comm_UART uartCommunication(Serial1);
ASRS_Slave uartSlave(uartCommunication);
ASRS_Slave *selectedSlave = nullptr;

static bool useEspNow = false;
static int32_t currentX = 100;
static int32_t currentZ = 50;

static void configureSlaveState(ASRS_Slave &slave) {
  slave.setCoordinates(currentX, currentZ);
  slave.setLimitSwitches(false, false, false, false);
  slave.setOperationStatus(ASRS_STATUS_IDLE);
}

static void updateSelectedSlave() {
  if (selectedSlave == nullptr) {
    return;
  }

#if defined(ESP32)
  if (useEspNow) {
    espNowSession.update();
    if (!espNowSession.connected()) {
      return;
    }

    if (espNowCommunication.available()) {
      espNowSession.recordPeerActivity();
    }
  }
#endif

  selectedSlave->update();
}

static void processTravelCommand() {
  if (selectedSlave == nullptr) {
    return;
  }

  ASRS_TravelCommand command;
  if (selectedSlave->readTravelCommand(command)) {
    currentX += command.xDistance;
    currentZ += command.zDistance;

    selectedSlave->setCoordinates(currentX, currentZ);
    selectedSlave->setOperationStatus(ASRS_STATUS_DONE);
    selectedSlave->publishOperationStatus();

    Serial.print("Travel command received: X distance = ");
    Serial.print(command.xDistance);
    Serial.print(", Z distance = ");
    Serial.println(command.zDistance);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  uartCommunication.begin(ASRS_UART_BAUD, ASRS_UART_RX_PIN, ASRS_UART_TX_PIN);

#if defined(ESP32)
  pinMode(ASRS_DIP_PIN, INPUT_PULLUP);
  useEspNow = digitalRead(ASRS_DIP_PIN) == ASRS_DIP_ESPNOW_LEVEL;

  if (useEspNow) {
    selectedSlave = &espNowSlave;
    configureSlaveState(*selectedSlave);

    if (!espNowSession.begin(true, ASRS_ESPNOW_CHANNEL, false, &Serial)) {
      Serial.print("ESP-NOW slave session initialisation failed: ");
      Serial.println(asrsErrorName(espNowSession.lastError()));
      selectedSlave = nullptr;
      return;
    }

    Serial.println("ASRS slave example started over ESP-NOW.");
    Serial.println("GPIO2 DIP rule: HIGH selects ESP-NOW; LOW selects UART.");
  } else {
    selectedSlave = &uartSlave;
    configureSlaveState(*selectedSlave);
    Serial.println("ASRS slave example started over UART.");
    Serial.println("GPIO2 DIP rule: HIGH selects ESP-NOW; LOW selects UART.");
  }
#else
  selectedSlave = &uartSlave;
  configureSlaveState(*selectedSlave);
  Serial.println("ASRS slave example started over UART.");
#endif
}

void loop() {
  updateSelectedSlave();
  processTravelCommand();
}
