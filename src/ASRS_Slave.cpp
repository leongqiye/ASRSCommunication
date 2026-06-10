#include "ASRS_Slave.h"

ASRS_Slave::ASRS_Slave(ASRS_Comm_Base &selectedCommunication, uint8_t nodeId)
  : _selectedCommunication(&selectedCommunication),
    _wrongModeCommunication(nullptr),
    _nodeId(nodeId),
    _sequenceCounter(1),
    _lastError(ASRS_ERROR_NONE),
    _hasTravelCommand(false) {
  _coordinates = {0, 0};
  _limits = {false, false, false, false};
  _status = {ASRS_STATUS_IDLE, ASRS_ERROR_NONE, 0};
  _travelCommand = {0, 0};
}

void ASRS_Slave::setWrongModeCommunication(ASRS_Comm_Base &communication) {
  _wrongModeCommunication = &communication;
}

void ASRS_Slave::update() {
  processSelectedCommunication();
  processWrongModeCommunication();
}

void ASRS_Slave::setCoordinates(int32_t x, int32_t z) {
  _coordinates.x = x;
  _coordinates.z = z;
}

void ASRS_Slave::setLimitSwitches(bool xMinimum, bool xMaximum, bool zMinimum, bool zMaximum) {
  _limits.xMinimum = xMinimum;
  _limits.xMaximum = xMaximum;
  _limits.zMinimum = zMinimum;
  _limits.zMaximum = zMaximum;
}

void ASRS_Slave::setOperationStatus(ASRS_Status status, ASRS_Error error, uint8_t warningCode) {
  _status.status = status;
  _status.error = error;
  _status.warningCode = warningCode;
}

bool ASRS_Slave::publishOperationStatus() {
  return sendStatus(*_selectedCommunication, 0);
}

bool ASRS_Slave::travelCommandAvailable() const {
  return _hasTravelCommand;
}

bool ASRS_Slave::readTravelCommand(ASRS_TravelCommand &command) {
  if (!_hasTravelCommand) {
    return false;
  }

  command = _travelCommand;
  _hasTravelCommand = false;
  return true;
}

ASRS_Error ASRS_Slave::lastError() const {
  return _lastError;
}

void ASRS_Slave::processSelectedCommunication() {
  while (_selectedCommunication->available()) {
    ASRS_Packet packet;
    if (_selectedCommunication->readPacket(packet)) {
      processPacket(*_selectedCommunication, packet, false);
    } else {
      _lastError = _selectedCommunication->lastError();
    }
  }
}

void ASRS_Slave::processWrongModeCommunication() {
  if (_wrongModeCommunication == nullptr || _wrongModeCommunication == _selectedCommunication) {
    return;
  }

  while (_wrongModeCommunication->available()) {
    ASRS_Packet packet;
    if (_wrongModeCommunication->readPacket(packet)) {
      processPacket(*_wrongModeCommunication, packet, true);
    } else {
      _lastError = _wrongModeCommunication->lastError();
    }
  }
}

void ASRS_Slave::processPacket(ASRS_Comm_Base &communication, ASRS_Packet &packet, bool wrongMode) {
  if (wrongMode) {
    sendError(communication, ASRS_ERROR_WRONG_MODE_SELECTED, packet.seq);
    _lastError = ASRS_ERROR_WRONG_MODE_SELECTED;
    return;
  }

  switch (packet.cmd) {
    case ASRS_CMD_REQ_COORDINATES:
      sendCoordinates(communication, packet.seq);
      break;

    case ASRS_CMD_REQ_LIMITS:
      sendLimitSwitches(communication, packet.seq);
      break;

    case ASRS_CMD_SET_MOVE:
      if (packet.len != 8) {
        sendError(communication, ASRS_ERROR_PAYLOAD_LENGTH, packet.seq);
        _lastError = ASRS_ERROR_PAYLOAD_LENGTH;
        return;
      }
      _travelCommand.xDistance = asrsReadInt32(&packet.data[0]);
      _travelCommand.zDistance = asrsReadInt32(&packet.data[4]);
      _hasTravelCommand = true;
      sendAcknowledgement(communication, packet.seq);
      break;

    default:
      sendError(communication, ASRS_ERROR_UNKNOWN_COMMAND, packet.seq);
      _lastError = ASRS_ERROR_UNKNOWN_COMMAND;
      break;
  }
}

bool ASRS_Slave::sendCoordinates(ASRS_Comm_Base &communication, uint8_t sequence) {
  ASRS_Packet packet;
  asrsClearPacket(packet);
  packet.id = _nodeId;
  packet.cmd = ASRS_CMD_RESP_COORDINATES;
  packet.seq = sequence;
  packet.len = 8;
  asrsWriteInt32(&packet.data[0], _coordinates.x);
  asrsWriteInt32(&packet.data[4], _coordinates.z);
  return communication.sendPacket(packet);
}

bool ASRS_Slave::sendLimitSwitches(ASRS_Comm_Base &communication, uint8_t sequence) {
  ASRS_Packet packet;
  asrsClearPacket(packet);
  packet.id = _nodeId;
  packet.cmd = ASRS_CMD_RESP_LIMITS;
  packet.seq = sequence;
  packet.len = 4;
  packet.data[0] = static_cast<uint8_t>(_limits.xMinimum);
  packet.data[1] = static_cast<uint8_t>(_limits.xMaximum);
  packet.data[2] = static_cast<uint8_t>(_limits.zMinimum);
  packet.data[3] = static_cast<uint8_t>(_limits.zMaximum);
  return communication.sendPacket(packet);
}

bool ASRS_Slave::sendAcknowledgement(ASRS_Comm_Base &communication, uint8_t sequence) {
  ASRS_Packet packet;
  asrsClearPacket(packet);
  packet.id = _nodeId;
  packet.cmd = ASRS_CMD_ACK;
  packet.seq = sequence;
  return communication.sendPacket(packet);
}

bool ASRS_Slave::sendStatus(ASRS_Comm_Base &communication, uint8_t sequence) {
  ASRS_Packet packet;
  asrsClearPacket(packet);
  packet.id = _nodeId;
  packet.cmd = ASRS_CMD_STATUS;
  packet.seq = sequence == 0 ? _sequenceCounter++ : sequence;
  packet.len = 3;
  packet.data[0] = static_cast<uint8_t>(_status.status);
  packet.data[1] = static_cast<uint8_t>(_status.error);
  packet.data[2] = _status.warningCode;
  return communication.sendPacket(packet);
}

bool ASRS_Slave::sendError(ASRS_Comm_Base &communication, ASRS_Error error, uint8_t sequence) {
  ASRS_Packet packet;
  asrsClearPacket(packet);
  packet.id = _nodeId;
  packet.cmd = ASRS_CMD_ERROR;
  packet.seq = sequence;
  packet.len = 1;
  packet.data[0] = static_cast<uint8_t>(error);
  return communication.sendPacket(packet);
}
