#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_VL53L1X.h>

static constexpr uint8_t ASRS_VL53L1X_TOF1_XSHUT_PIN = 10;
static constexpr uint8_t ASRS_VL53L1X_TOF2_XSHUT_PIN = 11;
static constexpr uint8_t ASRS_VL53L1X_TOF3_XSHUT_PIN = 12;
static constexpr uint8_t ASRS_VL53L1X_SDA_PIN = 8;
static constexpr uint8_t ASRS_VL53L1X_SCL_PIN = 9;
static constexpr uint8_t ASRS_VL53L1X_TOF1_ADDRESS = 0x64;
static constexpr uint8_t ASRS_VL53L1X_TOF2_ADDRESS = 0x65;
static constexpr uint8_t ASRS_VL53L1X_TOF3_ADDRESS = 0x66;
static constexpr uint8_t SENSOR_COUNT = 3;
static constexpr uint16_t INVALID_DISTANCE_MM = 65535;
static constexpr uint8_t INVALID_RANGE_STATUS = 255;
static constexpr uint16_t TIMING_BUDGET_MS = 50;
static constexpr uint32_t EXCLUSIVE_RANGING_INTERVAL_MS = 500;
static constexpr uint16_t SENSOR_BOOT_DELAY_MS = 350;
static constexpr uint16_t XSHUT_SETTLE_DELAY_MS = 50;
static constexpr uint16_t DATA_READY_TIMEOUT_MS = 150;

enum SensorTestMode
{
    TEST_EXCLUSIVE_RANGING,
    TEST_EXCLUSIVE_POWER
};

struct SensorConfig
{
    const char *label;
    uint8_t xshutPin;
    uint8_t i2cAddress;
};

struct SensorReading
{
    uint16_t distanceMm;
    uint8_t rangeStatus;
    bool valid;
};

static const SensorConfig SENSOR_CONFIG[SENSOR_COUNT] = {
    {"tof1", ASRS_VL53L1X_TOF1_XSHUT_PIN, ASRS_VL53L1X_TOF1_ADDRESS},
    {"tof3", ASRS_VL53L1X_TOF3_XSHUT_PIN, ASRS_VL53L1X_TOF3_ADDRESS},
    {"tof2", ASRS_VL53L1X_TOF2_XSHUT_PIN, ASRS_VL53L1X_TOF2_ADDRESS},
};

static SFEVL53L1X addressedSensors[SENSOR_COUNT];
static SensorTestMode testMode = TEST_EXCLUSIVE_RANGING;
static bool addressedSensorsReady = false;
static uint32_t nextMeasurementMs = 0;

static void scanI2CBus(const char *label)
{
    Serial.print("I2C scan ");
    Serial.print(label);
    Serial.print(":");

    bool foundDevice = false;
    for (uint8_t address = 1; address < 127; ++address)
    {
        Wire.beginTransmission(address);
        const uint8_t error = Wire.endTransmission();
        if (error == 0)
        {
            Serial.print(" 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.print(address, HEX);
            foundDevice = true;
        }
    }

    if (!foundDevice)
    {
        Serial.print(" none");
    }

    Serial.println();
}

static void shutdownAllSensors()
{
    for (uint8_t i = 0; i < SENSOR_COUNT; ++i)
    {
        digitalWrite(SENSOR_CONFIG[i].xshutPin, LOW);
    }
}

static void stopAllAddressedRanging()
{
    if (!addressedSensorsReady)
    {
        return;
    }

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i)
    {
        addressedSensors[i].stopRanging();
    }
}

static void printMode()
{
    Serial.print("Sensor test mode: ");
    if (testMode == TEST_EXCLUSIVE_RANGING)
    {
        Serial.println("exclusive ranging");
    }
    else
    {
        Serial.println("exclusive XSHUT power");
    }
}

static void printHelp()
{
    Serial.println("Commands:");
    Serial.println("  r = exclusive ranging test");
    Serial.println("  p = exclusive XSHUT power test");
    Serial.println("  h = show this help");
}

static bool configureSensor(SFEVL53L1X &sensor)
{
    sensor.setDistanceModeLong();
    sensor.setTimingBudgetInMs(TIMING_BUDGET_MS);
    delay(20);
    return true;
}

static bool initializeAddressedSensors()
{
    addressedSensorsReady = false;
    shutdownAllSensors();
    delay(500);
    scanI2CBus("after all XSHUT LOW");

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i)
    {
        digitalWrite(SENSOR_CONFIG[i].xshutPin, HIGH);
        delay(SENSOR_BOOT_DELAY_MS);
        scanI2CBus(SENSOR_CONFIG[i].label);

        if (addressedSensors[i].begin() != 0)
        {
            Serial.print("VL53L1X init failed for ");
            Serial.println(SENSOR_CONFIG[i].label);
            shutdownAllSensors();
            return false;
        }

        addressedSensors[i].setI2CAddress(SENSOR_CONFIG[i].i2cAddress);
        delay(50);
        configureSensor(addressedSensors[i]);

        Serial.print(SENSOR_CONFIG[i].label);
        Serial.print(" initialized at address 0x");
        Serial.println(SENSOR_CONFIG[i].i2cAddress, HEX);
    }

    addressedSensorsReady = true;
    return true;
}

static SensorReading readOneMeasurement(SFEVL53L1X &sensor)
{
    SensorReading reading = {INVALID_DISTANCE_MM, INVALID_RANGE_STATUS, false};

    sensor.startRanging();
    delay(5);

    const uint32_t startMs = millis();
    while (!sensor.checkForDataReady())
    {
        if (millis() - startMs > DATA_READY_TIMEOUT_MS)
        {
            sensor.stopRanging();
            return reading;
        }
        delay(1);
    }

    reading.rangeStatus = sensor.getRangeStatus();
    reading.distanceMm = sensor.getDistance();
    reading.valid = true;

    sensor.clearInterrupt();
    sensor.stopRanging();
    delay(10);

    return reading;
}

static SensorReading readAddressedSensor(uint8_t index)
{
    stopAllAddressedRanging();
    delay(5);
    SensorReading reading = readOneMeasurement(addressedSensors[index]);
    stopAllAddressedRanging();
    return reading;
}

static SensorReading readPowerCycledSensor(uint8_t index)
{
    SensorReading reading = {INVALID_DISTANCE_MM, INVALID_RANGE_STATUS, false};

    shutdownAllSensors();
    delay(XSHUT_SETTLE_DELAY_MS);

    digitalWrite(SENSOR_CONFIG[index].xshutPin, HIGH);
    delay(SENSOR_BOOT_DELAY_MS);

    SFEVL53L1X defaultAddressSensor(Wire);
    scanI2CBus(SENSOR_CONFIG[index].label);

    if (defaultAddressSensor.begin() != 0)
    {
        digitalWrite(SENSOR_CONFIG[index].xshutPin, LOW);
        delay(XSHUT_SETTLE_DELAY_MS);
        return reading;
    }

    configureSensor(defaultAddressSensor);
    reading = readOneMeasurement(defaultAddressSensor);

    digitalWrite(SENSOR_CONFIG[index].xshutPin, LOW);
    delay(XSHUT_SETTLE_DELAY_MS);

    return reading;
}

static void printReading(const SensorConfig &config, const SensorReading &reading)
{
    Serial.print(config.label);
    Serial.print(": ");

    if (!reading.valid || reading.distanceMm == INVALID_DISTANCE_MM)
    {
        Serial.print("invalid");
    }
    else
    {
        Serial.print(reading.distanceMm);
        Serial.print(" mm");
    }

    Serial.print(", status: ");
    Serial.println(reading.rangeStatus);
}

static void runExclusiveRangingCycle()
{
    if (!addressedSensorsReady && !initializeAddressedSensors())
    {
        Serial.println("Exclusive ranging initialization failed.");
        nextMeasurementMs = millis() + 1000;
        return;
    }

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i)
    {
        printReading(SENSOR_CONFIG[i], readAddressedSensor(i));
    }
}

static void runExclusivePowerCycle()
{
    addressedSensorsReady = false;

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i)
    {
        printReading(SENSOR_CONFIG[i], readPowerCycledSensor(i));
    }

    shutdownAllSensors();
}

static void setMode(SensorTestMode newMode)
{
    if (testMode == newMode)
    {
        printMode();
        return;
    }

    stopAllAddressedRanging();
    shutdownAllSensors();
    delay(100);

    testMode = newMode;
    addressedSensorsReady = false;
    nextMeasurementMs = 0;
    printMode();
}

static void processSerialCommand()
{
    while (Serial.available() > 0)
    {
        const char command = static_cast<char>(Serial.read());
        if (command == 'r' || command == 'R')
        {
            setMode(TEST_EXCLUSIVE_RANGING);
        }
        else if (command == 'p' || command == 'P')
        {
            setMode(TEST_EXCLUSIVE_POWER);
        }
        else if (command == 'h' || command == 'H')
        {
            printHelp();
            printMode();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("ASRS VL53L1X standalone sensor test.");
    Serial.println("I2C SDA: GPIO8, I2C SCL: GPIO9.");
    Serial.println("tof1 XSHUT: GPIO10, tof3 XSHUT: GPIO12, tof2 XSHUT: GPIO11.");
    Serial.println("Active sequence: tof1 -> tof3 -> tof2.");
    Serial.println("Exclusive ranging uses addresses 0x64, 0x66, and 0x65 in that sequence.");
    Serial.println("Exclusive power mode powers one sensor at a time and uses the default VL53L1X address.");

    Wire.begin(ASRS_VL53L1X_SDA_PIN, ASRS_VL53L1X_SCL_PIN);
    Wire.setClock(100000);

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i)
    {
        pinMode(SENSOR_CONFIG[i].xshutPin, OUTPUT);
        digitalWrite(SENSOR_CONFIG[i].xshutPin, LOW);
    }

    printHelp();
    printMode();
}

void loop()
{
    processSerialCommand();

    const uint32_t nowMs = millis();
    if (static_cast<int32_t>(nowMs - nextMeasurementMs) < 0)
    {
        return;
    }

    if (testMode == TEST_EXCLUSIVE_RANGING)
    {
        nextMeasurementMs = nowMs + EXCLUSIVE_RANGING_INTERVAL_MS;
        runExclusiveRangingCycle();
    }
    else
    {
        runExclusivePowerCycle();
        nextMeasurementMs = millis() + EXCLUSIVE_RANGING_INTERVAL_MS;
    }
}
