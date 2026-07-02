#include <Arduino.h>
#include <ASRSCommunication.h>
#include <U8g2lib.h>
#include <Wire.h>

#if !defined(ESP32)
#error "The ESP-NOW example requires an ESP32-compatible board."
#endif

static constexpr uint8_t ASRS_ESPNOW_CHANNEL = 1;
static constexpr uint32_t INITIAL_PAIRING_TIMEOUT_MS = 10000;

static constexpr uint8_t BUTTON_1_PIN = 14;
static constexpr uint8_t BUTTON_2_PIN = 13;
static constexpr uint8_t BUTTON_3_PIN = 12;
static constexpr uint8_t BUTTON_4_PIN = 11;
static constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
static constexpr int32_t BUTTON_2_TARGET_X_MM = 200;
static constexpr int32_t BUTTON_2_TARGET_Z_MM = 200;
static constexpr int32_t BUTTON_4_TARGET_X_MM = 100;
static constexpr int32_t BUTTON_4_TARGET_Z_MM = 100;
static constexpr uint8_t SERIAL_COMMAND_BUFFER_SIZE = 64;

static constexpr uint8_t OLED_SCL_PIN = 5;
static constexpr uint8_t OLED_SDA_PIN = 4;
static constexpr uint8_t OLED_RESET_PIN = U8X8_PIN_NONE;
static constexpr uint32_t OLED_REFRESH_MS = 200;
static const uint8_t *const OLED_TEXT_FONT = u8g2_font_5x8_tf;

ASRS_Comm_ESPNow espNowCommunication;
ASRS_ESPNow_MasterSession espNowSession(espNowCommunication);
ASRS_Master master(espNowCommunication);

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(
    U8G2_R0,
    OLED_RESET_PIN,
    OLED_SCL_PIN,
    OLED_SDA_PIN);

struct DebouncedButton {
  uint8_t pin;
  bool stableLevel;
  bool lastRawLevel;
  uint32_t lastChangeMs;
};

static DebouncedButton button1 = {BUTTON_1_PIN, HIGH, HIGH, 0};
static DebouncedButton button2 = {BUTTON_2_PIN, HIGH, HIGH, 0};
static DebouncedButton button3 = {BUTTON_3_PIN, HIGH, HIGH, 0};
static DebouncedButton button4 = {BUTTON_4_PIN, HIGH, HIGH, 0};

static int32_t lastCoordinateX = 0;
static int32_t lastCoordinateZ = 0;
static bool hasReceivedCoordinates = false;
static char slaveStatusText[24] = "Pairing";
static char slaveErrorText[24] = "None";
static char serialCommandBuffer[SERIAL_COMMAND_BUFFER_SIZE];
static uint8_t serialCommandLength = 0;
static uint32_t nextDisplayRefreshMs = 0;
static bool displayDirty = true;

static void setSlaveStatus(const char *statusText) {
  strncpy(slaveStatusText, statusText, sizeof(slaveStatusText) - 1);
  slaveStatusText[sizeof(slaveStatusText) - 1] = '\0';
  displayDirty = true;
}

static void setSlaveError(const char *errorText) {
  strncpy(slaveErrorText, errorText, sizeof(slaveErrorText) - 1);
  slaveErrorText[sizeof(slaveErrorText) - 1] = '\0';
  displayDirty = true;
}

static bool updateButton(DebouncedButton &button) {
  const bool rawLevel = digitalRead(button.pin);
  const uint32_t nowMs = millis();

  if (rawLevel != button.lastRawLevel) {
    button.lastRawLevel = rawLevel;
    button.lastChangeMs = nowMs;
  }

  if ((nowMs - button.lastChangeMs) < BUTTON_DEBOUNCE_MS ||
      rawLevel == button.stableLevel) {
    return false;
  }

  button.stableLevel = rawLevel;
  return button.stableLevel == LOW;
}

static void trimCommand(char *command) {
  char *start = command;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }

  if (start != command) {
    memmove(command, start, strlen(start) + 1);
  }

  size_t length = strlen(command);
  while (length > 0 &&
         (command[length - 1] == ' ' || command[length - 1] == '\t')) {
    command[length - 1] = '\0';
    --length;
  }
}

static void uppercaseCommand(char *command) {
  for (size_t i = 0; command[i] != '\0'; ++i) {
    if (command[i] >= 'a' && command[i] <= 'z') {
      command[i] = static_cast<char>(command[i] - 'a' + 'A');
    }
  }
}

static void drawDisplay() {
  if (!displayDirty &&
      static_cast<int32_t>(millis() - nextDisplayRefreshMs) < 0) {
    return;
  }

  nextDisplayRefreshMs = millis() + OLED_REFRESH_MS;
  displayDirty = false;

  char coordinateLine[40];
  char statusLine[40];
  char errorLine[40];
  ASRS_Coordinates storedCoordinates;
  if (master.lastCoordinates(storedCoordinates)) {
    lastCoordinateX = storedCoordinates.x;
    lastCoordinateZ = storedCoordinates.z;
    hasReceivedCoordinates = true;
  }

  if (hasReceivedCoordinates) {
    snprintf(coordinateLine,
             sizeof(coordinateLine),
             "Rx X:%ld Z:%ld",
             static_cast<long>(lastCoordinateX),
             static_cast<long>(lastCoordinateZ));
  } else {
    snprintf(coordinateLine,
             sizeof(coordinateLine),
             "Rx X:-- Z:--");
  }
  snprintf(statusLine,
           sizeof(statusLine),
           "slave status: %s",
           slaveStatusText);
  snprintf(errorLine,
           sizeof(errorLine),
           "error: %s",
           slaveErrorText);

  display.clearBuffer();
  display.setFont(OLED_TEXT_FONT);
  display.drawStr(0, 10, coordinateLine);
  display.drawStr(0, 24, statusLine);
  display.drawStr(0, 38, errorLine);
  display.sendBuffer();
}

static void requestCoordinatesFromSlave() {
  ASRS_Coordinates coordinates;

  if (!master.requestCoordinates(coordinates)) {
    Serial.print("Coordinate request failed: ");
    Serial.println(asrsErrorName(master.lastError()));
    setSlaveStatus(asrsErrorName(master.lastError()));
    setSlaveError(asrsErrorName(master.lastError()));
    return;
  }

  espNowSession.recordPeerActivity();
  ASRS_Coordinates storedCoordinates;
  if (master.lastCoordinates(storedCoordinates)) {
    lastCoordinateX = storedCoordinates.x;
    lastCoordinateZ = storedCoordinates.z;
    hasReceivedCoordinates = true;
  } else {
    lastCoordinateX = coordinates.x;
    lastCoordinateZ = coordinates.z;
    hasReceivedCoordinates = true;
  }
  setSlaveStatus("Coordinate");
  setSlaveError("None");

  Serial.print("Coordinates received: X = ");
  Serial.print(lastCoordinateX);
  Serial.print(", Z = ");
  Serial.println(lastCoordinateZ);
}

static void sendMoveCommand(int32_t targetX, int32_t targetZ) {
  if (master.sendTravelCommand(targetX, targetZ)) {
    espNowSession.recordPeerActivity();
    setSlaveStatus("ACK");
    setSlaveError("None");

    Serial.print("Target command acknowledged: X = ");
    Serial.print(targetX);
    Serial.print(", Z = ");
    Serial.println(targetZ);
    return;
  }

  Serial.print("Target command failed: ");
  Serial.println(asrsErrorName(master.lastError()));
  setSlaveStatus(asrsErrorName(master.lastError()));
  setSlaveError(asrsErrorName(master.lastError()));
}

static void updateActiveOperation() {
  if (!master.operationActive()) {
    return;
  }

  ASRS_OperationStatus status;
  if (!master.readOperationStatus(status)) {
    return;
  }

  espNowSession.recordPeerActivity();
  setSlaveStatus(asrsStatusName(status.status));
  setSlaveError(asrsErrorName(status.error));
  asrsPrintOperationStatus(Serial, status);

  if (status.status == ASRS_STATUS_DONE) {
    requestCoordinatesFromSlave();
  }
}

static void sendHomingCommandFromInput(bool xHome, bool zHome) {
  if (master.sendHomingCommand(xHome, zHome)) {
    espNowSession.recordPeerActivity();
    setSlaveStatus("ACK");
    setSlaveError("None");

    Serial.print("Homing command acknowledged: X = ");
    Serial.print(xHome ? "true" : "false");
    Serial.print(", Z = ");
    Serial.println(zHome ? "true" : "false");
    return;
  }

  Serial.print("Homing command failed: ");
  Serial.println(asrsErrorName(master.lastError()));
  setSlaveStatus(asrsErrorName(master.lastError()));
  setSlaveError(asrsErrorName(master.lastError()));
}

static bool commandRequiresConnectedSlave(const char *command) {
  if (!espNowSession.connected()) {
    Serial.print("Command rejected while pairing: ");
    Serial.println(command);
    setSlaveStatus("Pairing");
    setSlaveError("Not paired");
    return false;
  }
  return true;
}

static bool commandRequiresIdleMaster(const char *command) {
  if (master.operationActive()) {
    Serial.print("Command rejected while operation is active: ");
    Serial.println(command);
    setSlaveStatus("Master busy");
    setSlaveError(asrsErrorName(ASRS_ERROR_MASTER_BUSY));
    return false;
  }
  return true;
}

static void printSerialCommandHelp() {
  Serial.println("Serial commands:");
  Serial.println("  RC");
  Serial.println("  MOVE(x,z)");
  Serial.println("  HOME(x,z), where x and z are 0 or 1");
  Serial.println("  STATUS");
  Serial.println("  HELP");
}

static void printSerialStatus() {
  Serial.print("ESP-NOW paired: ");
  Serial.println(espNowSession.connected() ? "yes" : "no");
  Serial.print("Operation active: ");
  Serial.println(master.operationActive() ? "yes" : "no");
  Serial.print("Last error: ");
  Serial.println(asrsErrorName(master.lastError()));

  ASRS_Coordinates coordinates;
  if (master.lastCoordinates(coordinates)) {
    Serial.print("Last coordinates: X = ");
    Serial.print(coordinates.x);
    Serial.print(", Z = ");
    Serial.println(coordinates.z);
    return;
  }

  Serial.println("Last coordinates: not available");
}

static void processSerialCommand(char *command) {
  trimCommand(command);
  if (command[0] == '\0') {
    return;
  }

  uppercaseCommand(command);

  if (strcmp(command, "HELP") == 0) {
    printSerialCommandHelp();
    return;
  }

  if (strcmp(command, "STATUS") == 0) {
    printSerialStatus();
    return;
  }

  if (strcmp(command, "RC") == 0) {
    if (commandRequiresConnectedSlave(command) &&
        commandRequiresIdleMaster(command)) {
      requestCoordinatesFromSlave();
    }
    return;
  }

  long targetX = 0;
  long targetZ = 0;
  if (sscanf(command, "MOVE(%ld,%ld)", &targetX, &targetZ) == 2) {
    if (commandRequiresConnectedSlave(command) &&
        commandRequiresIdleMaster(command)) {
      sendMoveCommand(static_cast<int32_t>(targetX), static_cast<int32_t>(targetZ));
    }
    return;
  }

  int xHome = 0;
  int zHome = 0;
  if (sscanf(command, "HOME(%d,%d)", &xHome, &zHome) == 2) {
    if ((xHome != 0 && xHome != 1) ||
        (zHome != 0 && zHome != 1) ||
        (xHome == 0 && zHome == 0)) {
      Serial.println("Invalid HOME command. Use HOME(1,0), HOME(0,1), or HOME(1,1).");
      setSlaveError(asrsErrorName(ASRS_ERROR_INVALID_HOMING_REQUEST));
      return;
    }

    if (commandRequiresConnectedSlave(command) &&
        commandRequiresIdleMaster(command)) {
      sendHomingCommandFromInput(xHome != 0, zHome != 0);
    }
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(command);
  printSerialCommandHelp();
}

static void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char received = static_cast<char>(Serial.read());

    if (received == '\r' || received == '\n') {
      if (serialCommandLength > 0) {
        serialCommandBuffer[serialCommandLength] = '\0';
        processSerialCommand(serialCommandBuffer);
        serialCommandLength = 0;
      }
      continue;
    }

    if (serialCommandLength < SERIAL_COMMAND_BUFFER_SIZE - 1) {
      serialCommandBuffer[serialCommandLength++] = received;
    } else {
      serialCommandLength = 0;
      Serial.println("Serial command too long; input discarded.");
      setSlaveError("Command long");
    }
  }
}

static void handleButtons() {
  if (!espNowSession.connected()) {
    updateButton(button1);
    updateButton(button2);
    updateButton(button3);
    updateButton(button4);
    return;
  }

  if (updateButton(button1)) {
    requestCoordinatesFromSlave();
  }

  if (updateButton(button2)) {
    sendMoveCommand(BUTTON_2_TARGET_X_MM, BUTTON_2_TARGET_Z_MM);
  }

  if (updateButton(button3)) {
    sendHomingCommandFromInput(true, true);
  }

  if (updateButton(button4)) {
    sendMoveCommand(BUTTON_4_TARGET_X_MM, BUTTON_4_TARGET_Z_MM);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  pinMode(BUTTON_4_PIN, INPUT_PULLUP);

  display.begin();
  display.clearBuffer();
  display.setFont(OLED_TEXT_FONT);
  display.drawStr(0, 10, "ASRS master starting");
  display.sendBuffer();

  if (!espNowSession.begin(ASRS_ESPNOW_CHANNEL, INITIAL_PAIRING_TIMEOUT_MS, &Serial)) {
    Serial.print("ESP-NOW master session initialisation failed: ");
    Serial.println(asrsErrorName(espNowSession.lastError()));
    setSlaveStatus(asrsErrorName(espNowSession.lastError()));
    setSlaveError(asrsErrorName(espNowSession.lastError()));
    return;
  }

  Serial.println("ASRS ESP-NOW button master started with dynamic pairing.");
  Serial.println("Button 1: request coordinates.");
  Serial.println("Button 2: send target X = 200, Z = 200.");
  Serial.println("Button 3: send X/Z-axis homing command.");
  Serial.println("Button 4: send target X = 100, Z = 100.");
  Serial.println("Buttons use INPUT_PULLUP and are active LOW.");
  printSerialCommandHelp();
}

void loop() {
  espNowSession.update();

  if (espNowSession.connected()) {
    if (strcmp(slaveStatusText, "Pairing") == 0) {
      setSlaveStatus("Paired");
    }
  } else if (strcmp(slaveStatusText, "Pairing") != 0) {
    setSlaveStatus("Pairing");
  }

  if (espNowSession.connected()) {
    updateActiveOperation();
  }

  handleButtons();
  handleSerialCommands();

  drawDisplay();
}
