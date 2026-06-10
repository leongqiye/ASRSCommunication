#include "ASRS_Master.h"

ASRS_Master::ASRS_Master(ASRS_Comm_Base &communication, uint8_t nodeId)
  : _communication(&communication),
    _nodeId(nodeId),
    _sequenceCounter(1),
    _lastError(ASRS_ERROR_NONE) {
}

bool ASRS_Master::requestCoordinates(ASRS_Coordinates &coordinates, uint32_t timeoutMs) {
  //make a packet of request coordinate.
  ASRS_Packet request = makePacket(ASRS_CMD_REQ_COORDINATES);
  //if sendPacket success, would skip below block of code.
  if (!_communication->sendPacket(request)) {
    _lastError = _communication->lastError();
    return false;
  }
  //create a response packet.
  ASRS_Packet response;
  if (!waitForPacket(ASRS_CMD_RESP_COORDINATES, response, timeoutMs)) {
    return false;
  }

  return decodeCoordinates(response, coordinates);
}

bool ASRS_Master::requestLimitSwitches(ASRS_LimitSwitches &limits, uint32_t timeoutMs) {
  ASRS_Packet request = makePacket(ASRS_CMD_REQ_LIMITS);

  if (!_communication->sendPacket(request)) {
    _lastError = _communication->lastError();
    return false;
  }

  ASRS_Packet response;
  if (!waitForPacket(ASRS_CMD_RESP_LIMITS, response, timeoutMs)) {
    return false;
  }

  return decodeLimits(response, limits);
}

bool ASRS_Master::sendTravelCommand(int32_t xDistance, int32_t zDistance, uint32_t timeoutMs) {
  ASRS_Packet command = makePacket(ASRS_CMD_SET_MOVE);
  command.len = 8;
  asrsWriteInt32(&command.data[0], xDistance);
  asrsWriteInt32(&command.data[4], zDistance);

  if (!_communication->sendPacket(command)) {
    _lastError = _communication->lastError();
    return false;
  }

  ASRS_Packet acknowledgement;
  return waitForPacket(ASRS_CMD_ACK, acknowledgement, timeoutMs);
}

bool ASRS_Master::readOperationStatus(ASRS_OperationStatus &status) {
  if (!_communication->available()) {
    return false;
  }

  ASRS_Packet packet;
  if (!_communication->readPacket(packet)) {
    _lastError = _communication->lastError();
    return false;
  }

  if (packet.cmd == ASRS_CMD_STATUS || packet.cmd == ASRS_CMD_ERROR) {
    return decodeStatus(packet, status);
  }

  _lastError = ASRS_ERROR_UNKNOWN_COMMAND;
  return false;
}

ASRS_Error ASRS_Master::lastError() const {
  return _lastError;
}

ASRS_Packet ASRS_Master::makePacket(ASRS_Command command) {
  ASRS_Packet packet;
  asrsClearPacket(packet);
  packet.id = _nodeId;
  packet.cmd = static_cast<uint8_t>(command);
  packet.seq = _sequenceCounter++;
  return packet;
}

bool ASRS_Master::waitForPacket(uint8_t expectedCommand, ASRS_Packet &packet, uint32_t timeoutMs) {
  const uint32_t startTime = millis();

  while (millis() - startTime < timeoutMs) {
    if (!_communication->available()) {
      delay(1);
      continue;
    }

    if (!_communication->readPacket(packet)) {
      _lastError = _communication->lastError();
      return false;
    }

    if (packet.cmd == ASRS_CMD_ERROR) {
      ASRS_OperationStatus status;
      decodeStatus(packet, status);
      _lastError = status.error;
      return false;
    }

    if (packet.cmd == expectedCommand) {
      _lastError = ASRS_ERROR_NONE;
      return true;
    }
  }

  _lastError = ASRS_ERROR_TIMEOUT;
  return false;
}

bool ASRS_Master::decodeCoordinates(const ASRS_Packet &packet, ASRS_Coordinates &coordinates) {
  if (packet.len != 8) {
    _lastError = ASRS_ERROR_PAYLOAD_LENGTH;
    return false;
  }

  coordinates.x = asrsReadInt32(&packet.data[0]);
  coordinates.z = asrsReadInt32(&packet.data[4]);
  _lastError = ASRS_ERROR_NONE;
  return true;
}

bool ASRS_Master::decodeLimits(const ASRS_Packet &packet, ASRS_LimitSwitches &limits) {
  if (packet.len != 4) {
    _lastError = ASRS_ERROR_PAYLOAD_LENGTH;
    return false;
  }

  limits.xMinimum = packet.data[0] != 0;
  limits.xMaximum = packet.data[1] != 0;
  limits.zMinimum = packet.data[2] != 0;
  limits.zMaximum = packet.data[3] != 0;
  _lastError = ASRS_ERROR_NONE;
  return true;
}

bool ASRS_Master::decodeStatus(const ASRS_Packet &packet, ASRS_OperationStatus &status) {
  if (packet.cmd == ASRS_CMD_ERROR) {
    if (packet.len != 1) {
      _lastError = ASRS_ERROR_PAYLOAD_LENGTH;
      return false;
    }

    status.status = ASRS_STATUS_ERROR;
    status.error = static_cast<ASRS_Error>(packet.data[0]);
    status.warningCode = 0;
    _lastError = status.error;
    return true;
  }

  if (packet.len != 3) {
    _lastError = ASRS_ERROR_PAYLOAD_LENGTH;
    return false;
  }

  status.status = static_cast<ASRS_Status>(packet.data[0]);
  status.error = static_cast<ASRS_Error>(packet.data[1]);
  status.warningCode = packet.data[2];
  _lastError = status.error;
  return true;
}
