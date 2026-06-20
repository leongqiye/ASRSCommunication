#ifndef ASRS_SLAVE_H
#define ASRS_SLAVE_H

#include "ASRS_Comm_Base.h"

class ASRS_Slave {
public:
  ASRS_Slave(ASRS_Comm_Base &selectedCommunication, uint8_t nodeId = ASRS_DEFAULT_NODE_ID);

  void setWrongModeCommunication(ASRS_Comm_Base &communication);
  void update();

  void setCoordinates(int32_t x, int32_t z);
  void setLimitSwitches(bool xMinimum, bool xMaximum, bool zMinimum, bool zMaximum);
  void setOperationStatus(ASRS_Status status, ASRS_Error error = ASRS_ERROR_NONE, uint8_t warningCode = 0);
  bool publishOperationStatus();

  bool travelCommandAvailable() const;
  bool readTravelCommand(ASRS_TravelCommand &command);
  bool homingCommandAvailable() const;
  bool readHomingCommand(ASRS_HomingCommand &command);

  ASRS_Error lastError() const;

private:
  ASRS_Comm_Base *_selectedCommunication;
  ASRS_Comm_Base *_wrongModeCommunication;
  uint8_t _nodeId;
  uint8_t _sequenceCounter;
  ASRS_Error _lastError;

  ASRS_Coordinates _coordinates;
  ASRS_LimitSwitches _limits;
  ASRS_OperationStatus _status;
  //for handling travel command from master
  ASRS_TravelCommand _travelCommand;
  bool _hasTravelCommand;
  //for handling homing command from master
  ASRS_HomingCommand _homingCommand;
  bool _hasHomingCommand;

  void processSelectedCommunication();
  void processWrongModeCommunication();
  void processPacket(ASRS_Comm_Base &communication, ASRS_Packet &packet, bool wrongMode);

  bool sendCoordinates(ASRS_Comm_Base &communication, uint8_t sequence);
  bool sendLimitSwitches(ASRS_Comm_Base &communication, uint8_t sequence);
  bool sendAcknowledgement(ASRS_Comm_Base &communication, uint8_t sequence);
  bool sendStatus(ASRS_Comm_Base &communication, uint8_t sequence);
  bool sendError(ASRS_Comm_Base &communication, ASRS_Error error, uint8_t sequence);
};

#endif
