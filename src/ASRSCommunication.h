#ifndef ASRS_COMMUNICATION_H
#define ASRS_COMMUNICATION_H

#include "ASRS_Protocol.h"
#include "ASRS_Comm_Base.h"
#include "ASRS_FrameCodec.h"
#include "ASRS_Comm_UART.h"
#include "ASRS_Comm_ESPNow.h"
#include "ASRS_Master.h"
#include "ASRS_Slave.h"
#include "session/ASRS_ESPNow_MasterSession.h"
#include "session/ASRS_ESPNow_SlaveSession.h"
#include "VL53L1X_Manager.h"
//link error done
//sendHomingCommand(bool xHome, bool zHome, timeout) // if both true, home the Z first then follow by X.

//bool readLimitSwitches() -> return bool xRight,xLeft,zTop, zBottom

//research for the paring function for the espnow communication. urgent DONE IMPLEMENTED

//add the nema17 control code: include the convert linear to angular motion.

#endif
