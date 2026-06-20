#include <Arduino.h>
#include <ASRSCommunication.h>

// Arduino hardware UART master example.
// For Arduino Uno R4 boards, Serial1 uses fixed pins:
// D0 = RX, D1 = TX.
static constexpr uint32_t ASRS_UART_BAUD = 115200;
static constexpr uint32_t REQUEST_INTERVAL_MS = 2000;
static constexpr uint32_t COMMAND_INTERVAL_MS = 7000;

ASRS_Comm_UART uartCommunication(Serial1);
ASRS_Master master(uartCommunication);

static uint32_t lastRequestTime = 0;
static uint32_t lastCommandTime = 0;
static bool requestCoordinatesNext = true;
static int32_t commandedXDistance = 25;
static int32_t commandedZDistance = -10;

static void requestCoordinates() {
  ASRS_Coordinates coordinates;
  if (master.requestCoordinates(coordinates)) {
    Serial.print("Coordinate response: X = ");
    Serial.print(coordinates.x);
    Serial.print(" mm, Z = ");
    Serial.print(coordinates.z);
    Serial.println(" mm");
  } else {
    Serial.print("Coordinate request failed: ");
    Serial.println(asrsErrorName(master.lastError()));
  }
}

//update tp readLimitSwitches
static void requestLimitSwitches() {
  ASRS_LimitSwitches limits;
  if (master.requestLimitSwitches(limits)) {
    Serial.print("Limit response: Xmin = ");
    Serial.print(limits.xMinimum);
    Serial.print(", Xmax = ");
    Serial.print(limits.xMaximum);
    Serial.print(", Zmin = ");
    Serial.print(limits.zMinimum);
    Serial.print(", Zmax = ");
    Serial.println(limits.zMaximum);
  } else {
    Serial.print("Limit request failed: ");
    Serial.println(asrsErrorName(master.lastError()));
  }
}

static void sendTravelCommand() {
  if (master.sendTravelCommand(commandedXDistance, commandedZDistance)) {
    Serial.print("Travel command acknowledged: X distance = ");
    Serial.print(commandedXDistance);
    Serial.print(" mm, Z distance = ");
    Serial.print(commandedZDistance);
    Serial.println(" mm");
  } else {
    Serial.print("Travel command failed: ");
    Serial.println(asrsErrorName(master.lastError()));
  }

  commandedXDistance = -commandedXDistance;
  commandedZDistance = -commandedZDistance;
}

void setup() {
  Serial.begin(115200);
  const uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 2000) {
    delay(10);
  }

  // Non-ESP32 Arduino boards use fixed HardwareSerial pins.
  uartCommunication.begin(ASRS_UART_BAUD, -1, -1);

  Serial.println("ASRS UART master example for Arduino hardware UART started.");
  Serial.println("Uno R4 D1/TX shall connect to slave RX.");
  Serial.println("Uno R4 D0/RX shall connect to slave TX.");
  Serial.println("A common ground is required between boards.");
}

void loop() {
  const uint32_t now = millis();

  if (now - lastRequestTime >= REQUEST_INTERVAL_MS) {
    lastRequestTime = now;
    if (requestCoordinatesNext) {
      requestCoordinates();
    } else {
      requestLimitSwitches();
    }
    requestCoordinatesNext = !requestCoordinatesNext;
  }

  if (now - lastCommandTime >= COMMAND_INTERVAL_MS) {
    lastCommandTime = now;
    sendTravelCommand();
  }

  ASRS_OperationStatus status;
  if (master.readOperationStatus(status)) {
    Serial.print("Operation status: ");
    Serial.print(asrsStatusName(status.status));
    Serial.print(", error = ");
    Serial.print(asrsErrorName(status.error));
    Serial.print(", warning code = ");
    Serial.println(status.warningCode);
  }
}
