#ifndef __ASPCOMMON_H
#define __ASPCOMMON_H

/*
  aspCommon.h - Header library to contain constants needed for ASP

$Rev$
$LastChangedBy$
$LastChangedDate$
*/


// ASP identification
const char *aspSubsystem = "ASP";
const char *aspSerialNumber = "ASP01";
const char *aspVersion = "0.1";


// System information
const int nStands = 260;


// MCS communication
const int LOCAL_PORT = 1740;
const int REMOTE_PORT = 1741;
const char *REMOTE_IP = "10.1.1.2";


// Command que limits
const int CMD_QUEUE_SIZE = 32;

#endif

