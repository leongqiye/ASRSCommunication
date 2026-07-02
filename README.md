# ASRSCommunication

ASRSCommunication is an Arduino-compatible C++ library for the Automated Storage and Retrieval System (ASRS) communication protocol. The library contains the shared packet definitions, binary frame codec, UART transport, ESP-NOW transport, and master/slave role logic used by the ASRS controller firmware.

The current release is version `0.1.0`. This version number indicates that the library is usable for staged testing, but the public API and protocol may still evolve during research development.

## Library Layers

| Layer | Files | Responsibility |
| --- | --- | --- |
| Protocol | `ASRS_Protocol.h` | Command codes, status codes, error codes, packet structures, coordinate structures, limit switch structures, travel commands, and byte packing helpers. |
| Frame codec | `ASRS_FrameCodec.h`, `ASRS_FrameCodec.cpp` | Binary frame encoding, synchronization fields, protocol versioning, payload length validation, and CRC-16/CCITT verification. |
| Transport | `ASRS_Comm_UART.*`, `ASRS_Comm_ESPNow.*` | Physical communication over UART or ESP-NOW using the same packet and frame format. |
| Role logic | `ASRS_Master.*`, `ASRS_Slave.*` | Master and slave behaviours independent from the physical transport method. |
| Optional sensor support | `VL53L1X_Manager.*` | Helper for staged tower-board tests using SparkFun VL53L1X sensors. The communication library can be used without this dependency. |

## Arduino IDE Installation

Copy the complete `ASRSCommunication` folder into the Arduino libraries directory:

```text
Documents/Arduino/libraries/ASRSCommunication/
```

Restart the Arduino IDE after copying the folder. The examples should then appear under:

```text
File > Examples > ASRSCommunication
```

Install the external libraries required by the current examples through the Arduino Library Manager:

| Library | Used by |
| --- | --- |
| `U8g2` | `Master_ESPNow`, for the SSD1306 OLED display |
| `SparkFun VL53L1X 4m Laser Distance Sensor` | `Slave` and `Sensor_Test`, for VL53L1X time-of-flight sensors |
| `AccelStepper` | `Slave`, through the companion `ASRSMotion` motion-control library |

The `Slave` example also requires the local companion library `ASRSMotion`. Copy the complete `ASRSMotion` folder into `Documents/Arduino/libraries/` together with `ASRSCommunication` before compiling the slave sketch in the Arduino IDE.

## PlatformIO Usage

For a local PlatformIO project, place the library in the project `lib` directory:

```text
lib/ASRSCommunication/
```

The existing PlatformIO project already uses this layout.

## Maintaining The Library Inside A PlatformIO Project

During active development, this library can be maintained as a separate Git repository inside the PlatformIO project:

```text
Combined Communication Protocol/
└── lib/
    └── ASRSCommunication/   <- independent library repository
```

Use this workflow when changing the library:

1. Open a terminal in `lib/ASRSCommunication`.
2. Modify files in `src/`, `examples/`, or `docs/`.
3. Test the affected Arduino or PlatformIO examples.
4. Update the version in `library.properties` and `library.json` when the change is released.
5. Commit and push from the `ASRSCommunication` folder, not from the firmware project root.

The firmware project can continue to build against the local editable copy in `lib/ASRSCommunication`. When a stable release is required, tag the library repository and optionally replace the local copy with a `lib_deps` entry that points to the tagged GitHub version.

If the library is later published to a Git repository, a PlatformIO project can reference it in `platformio.ini`:

```ini
lib_deps =
  https://github.com/your-name/ASRSCommunication.git
```

## Basic UART Master Example

```cpp
#include <Arduino.h>
#include <ASRSCommunication.h>

ASRS_Comm_UART uartCommunication(Serial1);
ASRS_Master master(uartCommunication);

void setup() {
  Serial.begin(115200);
  uartCommunication.begin(115200, -1, -1);
}

void loop() {
  ASRS_Coordinates coordinates;
  if (master.requestCoordinates(coordinates)) {
    Serial.println(coordinates.x);
  }
  delay(1000);
}
```

On ESP32-S3 boards, pass explicit UART pins to `begin()`. On Arduino boards with fixed hardware UART pins, pass `-1` for both pin arguments.

## Included Examples

| Example | Target board type | Communication interface |
| --- | --- | --- |
| `Slave` | DIP-selected slave reference; GPIO2 HIGH selects ESP-NOW and GPIO2 LOW selects UART on ESP32 boards | UART or ESP-NOW |
| `Master_ESPNow` | ESP32 or ESP32-S3 boards only; uses dynamic ESP-NOW pairing | ESP-NOW |
| `Sensor_Test` | ESP32-S3 standalone VL53L1X diagnostic firmware | I2C sensor test |

The previous Arduino IDE UART master examples were removed from the packaged example set. PlatformIO UART test firmware may still exist in the wider research project, but it is no longer part of this Arduino IDE handover package.

## Versioning Policy

Use semantic versioning while the library develops:

| Change | Example | Version update |
| --- | --- | --- |
| Patch | Correct frame parsing without changing the API | `0.1.0` to `0.1.1` |
| Minor | Add a backward-compatible function or transport | `0.1.0` to `0.2.0` |
| Major | Change packet layout, command values, or public API behaviour | `0.1.0` to `1.0.0` or later major version |

Protocol-level changes must also be documented in `docs/ASRSCommunication_Library_Structure.txt` and in the project-level protocol documentation.
