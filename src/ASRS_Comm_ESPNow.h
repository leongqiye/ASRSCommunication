#ifndef ASRS_COMM_ESPNOW_H
#define ASRS_COMM_ESPNOW_H

#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#include <esp_now.h>
#endif
#include "ASRS_Comm_Base.h"
#include "ASRS_FrameCodec.h"

#if defined(ESP32)
class ASRS_Comm_ESPNow : public ASRS_Comm_Base {
public:
  ASRS_Comm_ESPNow();

  bool begin(const uint8_t peerAddress[6], uint8_t channel = 1);

  bool sendPacket(ASRS_Packet &packet) override;
  bool available() override;
  bool readPacket(ASRS_Packet &packet) override;
  ASRS_Medium medium() const override;
  ASRS_Error lastError() const override;
  static void processReceivedBytes(const uint8_t *data, int length);

private:
  uint8_t _peerAddress[6];
  uint8_t _sequenceCounter;
  ASRS_Error _lastError;
  bool _ready;

  static ASRS_Comm_ESPNow *_instance;
  static ASRS_Packet _pendingPacket;
  static bool _hasPendingPacket;
  static ASRS_Error _staticError;

};
#endif

#endif
