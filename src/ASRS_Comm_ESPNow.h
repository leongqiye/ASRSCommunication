#ifndef ASRS_COMM_ESPNOW_H
#define ASRS_COMM_ESPNOW_H

#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>
#endif
#include "ASRS_Comm_Base.h"
#include "ASRS_FrameCodec.h"

#if defined(ESP32)
class ASRS_Comm_ESPNow : public ASRS_Comm_Base {
public:
  ASRS_Comm_ESPNow();

  bool begin(const uint8_t peerAddress[6], uint8_t channel = 1);
  bool beginPairingMaster(uint8_t channel = 1);
  bool beginPairingSlave(bool pairingAllowed, uint8_t channel = 1, bool rememberPeer = true);
  bool pairAsMaster(uint32_t timeoutMs = 5000, uint32_t retryIntervalMs = 500);
  bool restartPairingMaster();
  bool clearStoredPeer();
  bool isPaired() const;
  void copyPeerAddress(uint8_t destination[6]) const;
  bool lastSendDelivered() const;
  uint8_t consecutiveDeliveryFailures() const;
  uint32_t millisecondsSinceLastDelivery() const;
  bool linkTimedOut(uint32_t timeoutMs) const;

  bool sendPacket(ASRS_Packet &packet) override;
  bool available() override;
  bool readPacket(ASRS_Packet &packet) override;
  ASRS_Medium medium() const override;
  ASRS_Error lastError() const override;
  static void processReceivedBytes(const uint8_t *data, int length);
  static void processReceivedBytes(const uint8_t *senderMac, const uint8_t *data, int length);
  static void processSendStatus(const uint8_t *peerMac, esp_now_send_status_t status);

private:
  enum PairingRole : uint8_t {
    PAIRING_ROLE_FIXED_PEER = 0,
    PAIRING_ROLE_MASTER = 1,
    PAIRING_ROLE_SLAVE = 2
  };

  uint8_t _peerAddress[6];
  uint8_t _sequenceCounter;
  ASRS_Error _lastError;
  bool _ready;
  bool _paired;
  bool _pairingAllowed;
  bool _rememberPeer;
  bool _storePeerPending;
  volatile bool _lastSendDelivered;
  volatile bool _sendStatusAvailable;
  volatile uint8_t _consecutiveDeliveryFailures;
  volatile uint32_t _lastDeliveryMs;
  uint8_t _channel;
  PairingRole _pairingRole;

  bool initialiseEspNow(uint8_t channel);
  bool addPeer(const uint8_t peerAddress[6]);
  bool sendPairingPacket(uint8_t command, const uint8_t destination[6]);
  bool loadStoredPeer();
  bool saveStoredPeer();
  void savePendingPeerIfNeeded();
  void resetDeliveryState();
  bool isBroadcastAddress(const uint8_t address[6]) const;
  bool isKnownPeer(const uint8_t senderMac[6]) const;
  void setPeerAddress(const uint8_t peerAddress[6]);
  void handleReceivedBytes(const uint8_t *senderMac, const uint8_t *data, int length);
  void handleSendStatus(const uint8_t *peerMac, esp_now_send_status_t status);

  static ASRS_Comm_ESPNow *_instance;
  static ASRS_Packet _pendingPacket;
  static bool _hasPendingPacket;
  static ASRS_Error _staticError;
  static const uint8_t BROADCAST_ADDRESS[6];

};
#endif

#endif
