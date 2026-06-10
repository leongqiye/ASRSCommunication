#include <Arduino.h>
#include <ASRSCommunication.h>

static constexpr uint32_t ASRS_UART_BAUD = 115200;

#if defined(ESP32)
static constexpr int8_t ASRS_UART_RX_PIN = 18;
static constexpr int8_t ASRS_UART_TX_PIN = 17;
#else
static constexpr int8_t ASRS_UART_RX_PIN = -1;
static constexpr int8_t ASRS_UART_TX_PIN = -1;
#endif

ASRS_Comm_UART uartCommunication(Serial1);
ASRS_Slave slave(uartCommunication);

static int32_t currentX = 100;
static int32_t currentZ = 50;

void setup() {
  Serial.begin(115200);
  delay(1000);

  uartCommunication.begin(ASRS_UART_BAUD, ASRS_UART_RX_PIN, ASRS_UART_TX_PIN);
  slave.setCoordinates(currentX, currentZ);
  slave.setLimitSwitches(false, false, false, false);
  slave.setOperationStatus(ASRS_STATUS_IDLE);

  Serial.println("ASRS UART slave example started.");
}

void loop() {
  slave.update();

  ASRS_TravelCommand command;
  if (slave.readTravelCommand(command)) {
    currentX += command.xDistance;
    currentZ += command.zDistance;

    slave.setCoordinates(currentX, currentZ);
    slave.setOperationStatus(ASRS_STATUS_DONE);
    slave.publishOperationStatus();

    Serial.print("Travel command received: X distance = ");
    Serial.print(command.xDistance);
    Serial.print(", Z distance = ");
    Serial.println(command.zDistance);
  }
}
