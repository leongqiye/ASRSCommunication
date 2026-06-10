#include "ASRS_Comm_ESPNow.h"

#if defined(ESP32)

ASRS_Comm_ESPNow *ASRS_Comm_ESPNow::_instance = nullptr;
ASRS_Packet ASRS_Comm_ESPNow::_pendingPacket = {};
bool ASRS_Comm_ESPNow::_hasPendingPacket = false;
ASRS_Error ASRS_Comm_ESPNow::_staticError = ASRS_ERROR_NONE;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void asrsEspNowReceiveCallback(const esp_now_recv_info_t *info, const uint8_t *data, int length) {
  (void)info;
  ASRS_Comm_ESPNow::processReceivedBytes(data, length);
}
#else
static void asrsEspNowReceiveCallback(const uint8_t *mac, const uint8_t *data, int length) {
  (void)mac;
  ASRS_Comm_ESPNow::processReceivedBytes(data, length);
}
#endif

ASRS_Comm_ESPNow::ASRS_Comm_ESPNow()
  : _sequenceCounter(0),
    _lastError(ASRS_ERROR_NONE),
    _ready(false) {
  memset(_peerAddress, 0xFF, sizeof(_peerAddress));
}

bool ASRS_Comm_ESPNow::begin(const uint8_t peerAddress[6], uint8_t channel) {
  memcpy(_peerAddress, peerAddress, sizeof(_peerAddress));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);

  if (esp_now_init() != ESP_OK) {
    _lastError = ASRS_ERROR_ESPNOW_NOT_AVAILABLE;
    return false;
  }

  _instance = this;
  esp_now_register_recv_cb(asrsEspNowReceiveCallback);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, _peerAddress, sizeof(peer.peer_addr));
  peer.channel = channel;
  peer.encrypt = false;

  if (!esp_now_is_peer_exist(_peerAddress) && esp_now_add_peer(&peer) != ESP_OK) {
    _lastError = ASRS_ERROR_ESPNOW_NOT_AVAILABLE;
    return false;
  }

  _ready = true;
  _lastError = ASRS_ERROR_NONE;
  return true;
}

bool ASRS_Comm_ESPNow::sendPacket(ASRS_Packet &packet) {
  if (!_ready) {
    _lastError = ASRS_ERROR_ESPNOW_NOT_AVAILABLE;
    return false;
  }

  if (packet.len > ASRS_MAX_DATA) {
    _lastError = ASRS_ERROR_PAYLOAD_LENGTH;
    return false;
  }

  if (packet.seq == 0) {
    packet.seq = _sequenceCounter++;
  }
  //The ESPNow working by sending packet in packet.
  //However, still encode it is to make the multiple medium share same packet format.
  //future compatibility, easier testing and application level error detection.
  uint8_t frame[ASRS_FrameCodec::MAX_FRAME_SIZE] = {};
  const size_t frameLength = ASRS_FrameCodec::encode(frame, packet);
  return esp_now_send(_peerAddress, frame, frameLength) == ESP_OK;
}

bool ASRS_Comm_ESPNow::available() {
  return _hasPendingPacket;
}

bool ASRS_Comm_ESPNow::readPacket(ASRS_Packet &packet) {
  if (!_hasPendingPacket) {
    return false;
  }

  packet = _pendingPacket;
  _hasPendingPacket = false;
  return true;
}

ASRS_Medium ASRS_Comm_ESPNow::medium() const {
  return ASRS_MEDIUM_ESPNOW;
}

ASRS_Error ASRS_Comm_ESPNow::lastError() const {
  return _staticError != ASRS_ERROR_NONE ? _staticError : _lastError;
}

void ASRS_Comm_ESPNow::processReceivedBytes(const uint8_t *data, int length) {
  if (data == nullptr || length <= 0) {
    _staticError = ASRS_ERROR_INVALID_FRAME;
    return;
  }

  ASRS_Error error = ASRS_ERROR_NONE;
  if (ASRS_FrameCodec::decode(data, static_cast<size_t>(length), _pendingPacket, error)) {
    _hasPendingPacket = true;
    _staticError = ASRS_ERROR_NONE;
  } else {
    _staticError = error;
  }
}

#endif
