#include "ASRS_ESPNow_MasterSession.h"

#if defined(ESP32)

ASRS_ESPNow_MasterSession::ASRS_ESPNow_MasterSession(ASRS_Comm_ESPNow &communication)
  : _communication(communication),
    _diagnosticStream(nullptr),
    _state(STATE_NOT_STARTED),
    _repairingTimeoutMs(3000),
    _linkTimeoutMs(6000),
    _lastPeerActivityMs(0),
    _linkLossPrinted(false) {
}

bool ASRS_ESPNow_MasterSession::begin(uint8_t channel,
                                      uint32_t initialPairingTimeoutMs,
                                      Stream *diagnosticStream) {
  _diagnosticStream = diagnosticStream;
  _state = STATE_PAIRING;
  _lastPeerActivityMs = 0;
  _linkLossPrinted = false;

  if (!_communication.beginPairingMaster(channel)) {
    return false;
  }

  if (!pairWithSlave(initialPairingTimeoutMs)) {
    printDiagnostic("Initial pairing did not complete. The master will keep retrying.");
  }

  return true;
}

void ASRS_ESPNow_MasterSession::update() {
  if (_state == STATE_NOT_STARTED) {
    return;
  }

  if (_state == STATE_PAIRING) {
    pairWithSlave(_repairingTimeoutMs);
    return;
  }

  if (hasLinkTimedOut()) {
    recoverPairingAfterLinkLoss();
  }
}

void ASRS_ESPNow_MasterSession::recordPeerActivity() {
  _lastPeerActivityMs = millis();
  _linkLossPrinted = false;
}

bool ASRS_ESPNow_MasterSession::connected() const {
  return _state == STATE_CONNECTED && _communication.isPaired();
}

bool ASRS_ESPNow_MasterSession::ready() const {
  return connected();
}

bool ASRS_ESPNow_MasterSession::pairing() const {
  return _state == STATE_PAIRING;
}

ASRS_Error ASRS_ESPNow_MasterSession::lastError() const {
  return _communication.lastError();
}

ASRS_Comm_ESPNow &ASRS_ESPNow_MasterSession::communication() {
  return _communication;
}

void ASRS_ESPNow_MasterSession::setRepairingTimeout(uint32_t timeoutMs) {
  _repairingTimeoutMs = timeoutMs;
}

void ASRS_ESPNow_MasterSession::setLinkTimeout(uint32_t timeoutMs) {
  _linkTimeoutMs = timeoutMs;
}

bool ASRS_ESPNow_MasterSession::pairWithSlave(uint32_t timeoutMs) {
  printDiagnostic("Broadcasting ASRS pair request.");
  if (!_communication.pairAsMaster(timeoutMs)) {
    if (_diagnosticStream != nullptr) {
      _diagnosticStream->print("Pairing attempt failed: ");
      _diagnosticStream->println(asrsErrorName(_communication.lastError()));
    }
    return false;
  }

  printPeerAddress("Paired slave MAC address: ");
  recordPeerActivity();
  _state = STATE_CONNECTED;
  return true;
}

bool ASRS_ESPNow_MasterSession::hasLinkTimedOut() const {
  if (_lastPeerActivityMs == 0) {
    return false;
  }

  const uint32_t elapsedSincePeerActivity = millis() - _lastPeerActivityMs;
  return elapsedSincePeerActivity > _linkTimeoutMs || _communication.linkTimedOut(_linkTimeoutMs);
}

void ASRS_ESPNow_MasterSession::recoverPairingAfterLinkLoss() {
  if (_diagnosticStream != nullptr && !_linkLossPrinted) {
    _diagnosticStream->print("Warning: ESP-NOW link timeout. No response from paired slave for ");
    _diagnosticStream->print(millis() - _lastPeerActivityMs);
    _diagnosticStream->print(" ms. Consecutive delivery failures = ");
    _diagnosticStream->println(_communication.consecutiveDeliveryFailures());
    _diagnosticStream->println("Master is returning to broadcast pairing mode.");
    _linkLossPrinted = true;
  }

  _state = STATE_PAIRING;
  _lastPeerActivityMs = 0;

  if (!_communication.restartPairingMaster() && _diagnosticStream != nullptr) {
    _diagnosticStream->print("Failed to restart master pairing mode: ");
    _diagnosticStream->println(asrsErrorName(_communication.lastError()));
  }
}

void ASRS_ESPNow_MasterSession::printPeerAddress(const char *label) {
  if (_diagnosticStream == nullptr) {
    return;
  }

  uint8_t peerAddress[6] = {};
  _communication.copyPeerAddress(peerAddress);
  _diagnosticStream->print(label);
  for (uint8_t i = 0; i < 6; ++i) {
    if (i > 0) {
      _diagnosticStream->print(":");
    }
    if (peerAddress[i] < 0x10) {
      _diagnosticStream->print("0");
    }
    _diagnosticStream->print(peerAddress[i], HEX);
  }
  _diagnosticStream->println();
}

void ASRS_ESPNow_MasterSession::printDiagnostic(const char *message) {
  if (_diagnosticStream != nullptr) {
    _diagnosticStream->println(message);
  }
}

#endif
