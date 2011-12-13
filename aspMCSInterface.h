#ifndef __ASPMCSINTERFACE_H
#define __ASPMCSINTERFACE_H

/*
  aspMCSInterface.h - Header library to deal with communicating with MCS
                      and handling commands, reporting on MIB entries, etc.

  Functions defined:
   * initMIB - create the default MIB database
   * interpretCommand - process in incoming MCS packet
   * generateResponse - respond to a MCS packet

$Rev$
$LastChangedBy$
$LastChangedDate$
*/

#include aspMIB.h
#include aspCommon.h
#include aspFunctions.h


// MCS message structure
const int tstart_DEST = 0;
const int tlength_DEST = 3;
const int tstart_SNDR = 3;
const int tlength_SNDR = 3;
const int tstart_TYPE = 6;
const int tlength_TYPE = 3;
const int tstart_REF = 9;
const int tlength_REF = 9;
const int tstart_DLEN = 18;
const int tlength_DLEN = 4;
const int tstart_MJD = 22;
const int tlength_MJD = 6;
const int tstart_MPM = 28;
const int tlength_MPM = 9;
const int tstart_DATA = 38;


// Buffers for received data
char RX_DEST[4];
char RX_SNDR[4];
char RX_TYPE[4];
char RX_REF[10];
char RX_DLEN[5];
char RX_MJD[7];
char RX_MPM[10];
char RX_DATA[257];
char TX_DATA[257];


// Function definitions
int initMIB(aspMIB*);
int interpretCommand(aspMIB*, aspCommandQueue*, char*);
int generateResponse(aspMIB*, unsigned long int, int, char*);


/*
 initMIB - Function to initialize a MIB structure with default values
*/

int initMIB(aspMIB* mib) {
	auto int i;

     printf("Initializing the MIB...");

     // Initialize the control variables
     mib->init = 0;
     mib->nBoards = 0;
     mib->nChP = 0;

	//Initialize MIB entries
	strcpy(mib->index1.summary, "SHUTDWN");
	strcpy(mib->index1.info, "ASP booted, but has not been initialized");
	strcpy(mib->index1.lastlog, "No LASTLOG exists");
	strcpy(mib->index1.subsystem, aspSubsystem);
	strcpy(mib->index1.serialno, aspSerialNumber);
	strcpy(mib->index1.version, aspVersion);

	strcpy(mib->index2.ARXSUPPLY, "OFF");
	strcpy(mib->index2.ARXSUPPLY_NO, " 1");
	strcpy(mib->index2.ARXPWRUNIT_1, "This is mock info about the ARX power supply");
	strcpy(mib->index2.ARXCURR, "      0");
	strcpy(mib->index2.FEESUPPLY, "OFF");
	strcpy(mib->index2.FEESUPPLY_NO, " 1");
	strcpy(mib->index2.FEEPWRUNIT_1, "This is mock info about the FEE power supply");
	strcpy(mib->index2.FEECURR, "      0");

	for(i=1; i<=260; i++) {
		mib->index3.FILTER[i-1] = 0;
		mib->index4.AT1[i-1] = 30;
		mib->index4.AT2[i-1] = 30;
		mib->index4.ATS[i-1] = 30;
		mib->index5.FEEPOL1PWR[i-1] = 0;
		mib->index5.FEEPOL2PWR[i-1] = 0;
	}

	strcpy(mib->index6.TEMP_STATUS, "IN_RANGE");
	strcpy(mib->index6.TEMP_SENSE_NO, "  1");
	strcpy(mib->index6.SENSOR_NAME_1, "mock sensor name");
	strcpy(mib->index6.SENSOR_DATA_1, "        25");

     printf("Done!\n");

     return 1;
}


/*
 interpertCommand - deal with command contained in an incoming MCS packet
*/

int interpertCommand(aspMIB* mib, aspCommandQueue* commandQueue, char* buf) {
	int tlength_DATA;
	int i, accepted, returnCode;
	int Stand, Pol, State;
    	int nFset, nATset;

	//Define packet message index & length


	// Cycle through the message and strip off data
	strncpy(RX_DEST, buf+tstart_DEST, tlength_DEST);
	RX_DEST[tlength_DEST] = '\0';
	// printf("  -> destination %s\n", RX_DEST);

	strncpy(RX_SNDR, buf+tstart_SNDR, tlength_SNDR);
	RX_SNDR[tlength_SNDR] = '\0';
	// printf("  -> sender %s\n", RX_SNDR);

	strncpy(RX_TYPE, buf+tstart_TYPE, tlength_TYPE);
	RX_TYPE[tlength_TYPE] = '\0';
	// printf("  -> type %s\n", RX_TYPE);

	strncpy(RX_REF, buf+tstart_REF, tlength_REF);
	RX_REF[tlength_REF] = '\0';
	// printf("  -> reference %s\n", RX_REF);

	strncpy(RX_DLEN, buf+tstart_DLEN, tlength_DLEN);
	RX_DLEN[tlength_DLEN] = '\0';
	// printf("  -> datalen %s\n", RX_DLEN);

	strncpy(RX_MJD, buf+tstart_MJD, tlength_MJD);
	RX_MJD[tlength_MJD] = '\0';
	// printf("  -> MJD %s\n", RX_MJD);

	strncpy(RX_MPM, buf+tstart_MPM, tlength_MPM);
	RX_MPM[tlength_MPM] = '\0';
	// printf("  -> MPM %s\n", RX_MPM);

	// Get the data portion of the message
	tlength_DATA = strtol(RX_DLEN, NULL, 10);
	if (tlength_DATA > 0) {
		strncpy(RX_DATA, buf+tstart_DATA, tlength_DATA);
		RX_DATA[tlength_DATA] = '\0';
		// printf("  -> data '%s'\n", RX_DATA);
	} else {
		// printf("  -> data section empty\n");
	}

     // Check the destination (ASP or ALL)
     if( strncmp(RX_DEST, aspSubsystem, 3) && strncmp(RX_DEST, "ALL", 3) ) {
		// printf("Command destined for %s, not %s or ALL, skipping\n", RX_DEST, aspSubsystem);
		return -1;
	}

	// PNG
	if( !strncmp(RX_TYPE, "PNG", 3) ) {
		// printf("Received a Ping \n");

		returnCode = 1;
		strcpy(TX_DATA, "");;

	// RPT
	} else if( !strncmp(RX_TYPE, "RPT", 3) ) {
		// printf("Reporting on MIB Entry -> %s \n", RX_DATA);
		returnCode = 1;

		if (!strcmp(RX_DATA, "SUMMARY"))
			strcpy(TX_DATA, mib->index1.summary);      //add the summary entry to data
		else if (!strcmp(RX_DATA, "INFO"))
			strcpy(TX_DATA, mib->index1.info);
		else if (!strcmp(RX_DATA, "LASTLOG"))
			strcpy(TX_DATA, mib->index1.lastlog);
		else if (!strcmp(RX_DATA, "SUBSYSTEM"))
			strcpy(TX_DATA, mib->index1.subsystem);
		else if (!strcmp(RX_DATA, "SERIALNO"))
			strcpy(TX_DATA, mib->index1.serialno);
		else if (!strcmp(RX_DATA, "VERSION"))
			strcpy(TX_DATA, mib->index1.version);
		else if (!strcmp(RX_DATA, "ARXSUPPLY"))
			strcpy(TX_DATA, mib->index2.ARXSUPPLY);
		else if (!strcmp(RX_DATA, "ARXSUPPLY-NO"))
			strcpy(TX_DATA, mib->index2.ARXSUPPLY_NO);
		else if (!strcmp(RX_DATA, "ARXPWRUNIT_1"))
			strcpy(TX_DATA, mib->index2.ARXPWRUNIT_1);
		else if (!strcmp(RX_DATA, "ARXCURR"))
			strcpy(TX_DATA, mib->index2.ARXCURR);
		else if (!strcmp(RX_DATA, "FEESUPPLY"))
			strcpy(TX_DATA, mib->index2.FEESUPPLY);
		else if (!strcmp(RX_DATA, "FEESUPPLY_NO"))
			strcpy(TX_DATA, mib->index2.FEESUPPLY_NO);
		else if (!strcmp(RX_DATA, "FEEPWRUNIT_1"))
			strcpy(TX_DATA, mib->index2.FEEPWRUNIT_1);
		else if (!strcmp(RX_DATA, "FEECURR"))
			strcpy(TX_DATA, mib->index2.FEECURR);
		//RPT FILTER
		else if (strstr(RX_DATA, "FILTER") != 0) {
			sscanf(RX_DATA, "FILTER_%3d", &Stand);

			if(1 <= Stand <= mib->nStands) {
				if( mib->index3.FILTER[Stand-1] == 0 ) {
					sprintf(TX_DATA, "%1d", 3);
				} else if( mib->index3.FILTER[Stand-1] == 3 ) {
					sprintf(TX_DATA, "%1d", 0);
				} else {
					sprintf(TX_DATA, "%1d", mib->index3.FILTER[Stand-1]);
				}

			} else {
				returnCode = 0;
				strcpy(TX_DATA, "Stand number out-of-range");
			}
		}
	  	//RPT AT1
	  	else if (strstr(RX_DATA, "AT1") != 0) {
			sscanf(RX_DATA, "AT1_%3d", &Stand);

			if(1 <= Stand <= mib->nStands){
				sprintf(TX_DATA, "%02d", mib->index4.AT1[Stand-1] / 2);

			} else {
				returnCode = 0;
				strcpy(TX_DATA, "Stand number out-of-range");
			}
		}
		//RPT AT2
		else if (strstr(RX_DATA, "AT2") != 0) {
			sscanf(RX_DATA, "AT2_%3d", &Stand);

			if(1 <= Stand <= mib->nStands) {
				sprintf(TX_DATA, "%02d", mib->index4.AT2[Stand-1] / 2);

			} else {
				returnCode = 0;
				strcpy(TX_DATA, "Stand number out-of-range");
			}
		}
		//RPT AT Split
		else if (strstr(RX_DATA, "ATSPLIT") != 0) {
			sscanf(RX_DATA, "ATSPLIT_%3d", &Stand);

			if(1 <= Stand <= mib->nStands) {
				sprintf(TX_DATA, "%02d", mib->index4.ATS[Stand-1] / 2);

			} else {
                    returnCode = 0;
				strcpy(TX_DATA, "Stand number out-of-range");
			}
		}
		//RPT FEE Pol 1 Pwr
		else if (strstr(RX_DATA, "FEEPOL1PWR") != 0) {
			sscanf(RX_DATA, "FEEPOL1PWR_%3d", &Stand);

			if(1 <= Stand <= mib->nStands) {
				if( mib->index5.FEEPOL1PWR[Stand-1] ) {
					strcpy(TX_DATA, "ON ");
				} else {
					strcpy(TX_DATA, "OFF");
				}

			} else {
                    returnCode = 0;
				strcpy(TX_DATA, "Stand number out-of-range");
			}
		}
		//RPT FEE Pol 2 Pwr
		else if (strstr(RX_DATA, "FEEPOL2PWR") != 0) {
			sscanf(RX_DATA, "FEEPOL2PWR_%3d", &Stand);

			if(1 <= Stand <= mib->nStands) {
				if( mib->index5.FEEPOL2PWR[Stand-1] ) {
					strcpy(TX_DATA, "ON ");
				} else {
					strcpy(TX_DATA, "OFF");
				}

			} else {
				returnCode = 0;
				strcpy(TX_DATA, "Stand number out-of-range");
			}
		}
		else if (!strcmp(RX_DATA, "TEMP-STATUS"))
			strcpy(TX_DATA, mib->index6.TEMP_STATUS);
		else if (!strcmp(RX_DATA, "TEMP-SENSE-NO"))
			strcpy(TX_DATA, mib->index6.TEMP_SENSE_NO);
		else if (!strcmp(RX_DATA, "SENSOR-NAME-1"))
			strcpy(TX_DATA, mib->index6.SENSOR_NAME_1);
		else if (!strcmp(RX_DATA, "SENSOR-DATA-1"))
			strcpy(TX_DATA, mib->index6.SENSOR_DATA_1);
		else {
			returnCode = 0;
			sprintf(TX_DATA, "Invalid MIB Entry '%s'", RX_DATA);
		}

	/*
	 Commands down here
	*/

	// INI command
	} else if( !strncmp(RX_TYPE, "INI", 3) ) {
		if (mib->init) {	//ASP already initialized
			returnCode = 0;
			strcpy(TX_DATA, "ASP already initialized (issue SHT, then INI to reinitialize)");
		} else {
			mib->nBoards = strtol(RX_DATA, NULL, 10);
			if(0 < mib->nBoards && mib->nBoards < 34) {  //Do it
               	mib->nStands = 8*mib->nBoards;
                    if( mib->nStands > 260 ) {
                    	mib->nStands = 260;
                    }
				mib->nChP = 8*mib->nBoards;
				mib->init = 1;

				returnCode = 1;
				strcpy(TX_DATA, "");

				accepted = addToQueueHighPriority(commandQueue, "initASP", 0, 0, 0, mib->nChP);

				// Make sure it make it into the queue
				if( !accepted ) {
					returnCode = 0;
					strcpy(TX_DATA, "SPI bus queue is full, re-submit command in 1 second");
				} else {
					strcpy(mib->index1.summary, "BOOTING");
					strcpy(mib->index1.info, "Initializing ASP, please wait");
				}

			} else {
				returnCode = 0;
				strcpy(TX_DATA, "ARX boards installed out of range");
			}
		}

	// FIL command
	} else if( !strncmp(RX_TYPE, "FIL", 3) ) {
		if (!mib->init) {
               returnCode = 0;
			strcpy(TX_DATA, "ASP needs to be initialized");
		} else {
			// printf("Filter Command Received, Data -> %s \n", RX_DATA);

			if (strlen(RX_DATA) == 5) {
				returnCode = 1;
				strcpy(TX_DATA, "");

				sscanf(RX_DATA, "%3d%2d", &Stand, &nFset);

				if(nFset == 0 && (0 <= Stand && Stand <= mib->nStands) ) { //split BW
					accepted = addToQueue(commandQueue, "setFilter", 3, 0, Stand, mib->nChP);
				} else if (nFset == 1 && (0 <= Stand && Stand <= mib->nStands) ) { //full BW
					accepted = addToQueue(commandQueue, "setFilter", 1, 0, Stand, mib->nChP);
				} else if (nFset == 2 && (0 <= Stand && Stand <= mib->nStands) ) { //reduced BW
					accepted = addToQueue(commandQueue, "setFilter", 2, 0, Stand, mib->nChP);
				} else if (nFset == 3 && (0 <= Stand && Stand <= mib->nStands) ) { //filter off
					accepted = addToQueue(commandQueue, "setFilter", 0, 0, Stand, mib->nChP);
				} else {	//Stand Number out of range
					returnCode = 0;
                         sprintf(TX_DATA, "Filter (%i) and/or stand (%i) setting out of range", nFset, Stand);
				}

				// Make sure it make it into the queue
				if( !accepted && returnCode ) {
					returnCode = 0;
					strcpy(TX_DATA, "SPI bus queue is full, re-submit command in 1 second");
				}

			} else {
               	returnCode = 0;
				strcpy(TX_DATA, "FIL data field out of range");
			}
		}

	// AT1 command
	} else if(!strncmp(RX_TYPE, "AT1", 3)) {
		if (!mib->init) {
			returnCode = 0;
			strcpy(TX_DATA, "ASP needs to be initialized");
		} else {
			// printf("AT1 Command Received, Data -> %s \n", RX_DATA);

			if (strlen(RX_DATA) == 5) {
				returnCode = 1;
				strcpy(TX_DATA, "");

				sscanf(RX_DATA, "%3d%2d", &Stand, &nATset);

                    if( (0 <= Stand && Stand <= mib->nStands) && (0 <= nATset && nATset <= 30) ) {
                    	accepted = addToQueue(commandQueue, "setAT1", 2*nATset, 0, Stand, mib->nChP);

					// Make sure it make it into the queue
					if( !accepted ) {
						returnCode = 0;
						strcpy(TX_DATA, "SPI bus queue is full, re-submit command in 1 second");
					}
                    } else {
                    	returnCode = 0;
					sprintf(TX_DATA, "AT1 (%i) and/or stand (%i) setting out of range", nATset, Stand);
                    }

			} else {
               	returnCode = 0;
				strcpy(TX_DATA, "AT1 data field out of range");
			}
		}

	// AT2 command
	} else if(!strncmp(RX_TYPE, "AT2", 3)) {
		if (!mib->init) {
			returnCode = 0;
			strcpy(TX_DATA, "ASP needs to be initialized");
		} else {
			// printf("AT2 Command Received, Data -> %s \n", RX_DATA);

			if (strlen(RX_DATA) == 5) {
				returnCode = 1;
				strcpy(TX_DATA, "");

				sscanf(RX_DATA, "%3d%2d", &Stand, &nATset);

                    if( (0 <= Stand && Stand <= mib->nStands) && (0 <= nATset && nATset <= 30) ) {
                    	accepted = addToQueue(commandQueue, "setAT2", 2*nATset, 0, Stand, mib->nChP);

					// Make sure it make it into the queue
					if( !accepted ) {
						returnCode = 0;
						strcpy(TX_DATA, "SPI bus queue is full, re-submit command in 1 second");
					}
                    } else {
                         returnCode = 0;
					sprintf(TX_DATA, "AT2 (%i) and/or stand (%i) setting out of range", nATset, Stand);
                    }

			} else {
				returnCode = 0;
				strcpy(TX_DATA, "AT2 data field out of range");
			}
		}

	// ATS command
	} else if(!strncmp(RX_TYPE, "ATS", 3)) {
		if (!mib->init) {
			returnCode = 0;
			strcpy(TX_DATA, "ASP needs to be initialized");
		} else {
			// printf("ATS Command Received, Data -> %s \n", RX_DATA);

			if (strlen(RX_DATA) == 5) {
				returnCode = 1;
				strcpy(TX_DATA, "");

				sscanf(RX_DATA, "%3d%2d", &Stand, &nATset);

                    if( (0 <= Stand && Stand <= mib->nStands) && (0 <= nATset && nATset <= 30) ) {
                    	accepted = addToQueue(commandQueue, "setATS", 2*nATset, 0, Stand, mib->nChP);

					// Make sure it make it into the queue
					if( !accepted ) {
						returnCode = 0;
						strcpy(TX_DATA, "SPI bus queue is full, re-submit command in 1 second");
					}
                    } else {
                    	returnCode = 0;
                         sprintf(TX_DATA, "ATS (%i) and/or stand (%i) setting out of range", nATset, Stand);
                    }

			} else {
               	returnCode = 0;
				strcpy(TX_DATA, "ATS data field out of range");
			}
		}

	// FPW command
	} else if(!strncmp(RX_TYPE, "FPW", 3)) {
		if (!mib->init) {
			returnCode = 0;
			strcpy(TX_DATA, "ASP needs to be initialized");
		} else {
			// printf("FPW Command Received, Data -> %s \n", RX_DATA);

			if (strlen(RX_DATA) == 6) {
				returnCode = 1;
				strcpy(TX_DATA, "");

				sscanf(RX_DATA, "%3d%1d%2d", &Stand, &Pol, &State);

				if( Pol == 1 && (0 <= Stand && Stand <= mib->nStands) ) {	//Polarization 1
					if( State == 11 ) {
						accepted = addToQueue(commandQueue, "setFEEPower", 1, 1, Stand, mib->nChP);
					} else if( State == 0 ) {
						accepted = addToQueue(commandQueue, "setFEEPower", 1, 0, Stand, mib->nChP);
					} else {
						returnCode = 0;
						sprintf(TX_DATA, "FPW power (%i) and/or stand (%i) setting out of range", State, Stand);
					}

					// Make sure it make it into the queue
					if( !accepted && returnCode ) {
						returnCode = 0;
						strcpy(TX_DATA, "SPI bus queue is full, re-submit command in 1 second");
					}

				} else if( Pol == 2 && (0 <= Stand && Stand <= mib->nStands) ) {	//Polarization 2
					if ( State == 11 ) {
						accepted = addToQueue(commandQueue, "setFEEPower", 2, 1, Stand, mib->nChP);
					} else if( State == 0 ) {
						accepted = addToQueue(commandQueue, "setFEEPower", 2, 0, Stand, mib->nChP);
					} else {
						returnCode = 0;
						sprintf(TX_DATA, "FPW power (%i) and/or stand (%i) setting out of range", State, Stand);
					}

					// Make sure it make it into the queue
					if( !accepted && returnCode ) {
						returnCode = 0;
						strcpy(TX_DATA, "SPI bus queue is full, re-submit command in 1 second");
					}

				} else { 	//FPW polarization out of range
					returnCode = 0;
					strcpy(TX_DATA, "FPW polarization and/or stand out of range");
				}

			}
		}

	// RXP command *** not implemented ***
	} else if(!strncmp(RX_TYPE, "RXP", 3)) {
		if (!mib->init) {
			returnCode = 0;
			strcpy(TX_DATA, "ASP needs to be initialized");
		} else {
               returnCode = 0;
			strcpy(TX_DATA, "RXP is not implemented in this version of ASP-MCS");
		}

	// FEP command *** not implemented ***
	} else if(!strncmp(RX_TYPE, "FEP", 3)) {
		if (!mib->init) {
			returnCode = 0;
			strcpy(TX_DATA, "ASP needs to be initialized");
		} else {
               returnCode = 0;
			strcpy(TX_DATA, "FEP is not implemented in this version of ASP-MCS");
		}

	// SHT command
	} else if(!strncmp(RX_TYPE, "SHT", 3)) {
		if (!mib->init) {
			returnCode = 0;
			strcpy(TX_DATA, "ASP needs to be initialized");
		} else {
			returnCode = 1;
			strcpy(TX_DATA, "");

			if( strncmp(RX_DATA, "SCRAM", 4) ) {
				accepted = addToQueueHighPriority(commandQueue, "haltASP", 0, 0, 0, mib->nChP);
			} else {
				accepted = addToQueue(commandQueue, "haltASP", 0, 0, 0, mib->nChP);
			}

			// Make sure it make it into the queue
			if( !accepted ) {
				returnCode = 0;
				strcpy(TX_DATA, "SPI bus queue is full, re-submit command in 1 second");
			} else {
				strcpy(mib->index1.summary, "SHUTDWN");
				strcpy(mib->index1.info, "Shutting down ASP, please wait");
			}

		}

	// Reject unknown commands
	} else {
			returnCode = 0;
			sprintf(TX_DATA, "Invalid Command '%s'", RX_TYPE);
	}

	// printf("%i -> %s\n", returnCode, TX_DATA);
	return returnCode;
}


/*
 generateResponse - Generate the reply to a MCS packet
*/

int generateResponse(aspMIB* mib, unsigned long int deltaT, int statusCode, char* buf) {
	char status[2];
	auto unsigned long int mjd, mpm;

     // Interperate the status code (0 -> R; 1 -> A)
     if( statusCode ) {
          strcpy(status, "A");
     } else {
          strcpy(status, "R");
     }

     // Calculate the new MJD/MPM based on the timer
     // change + some small delta T (say 1 ms)
     sscanf(RX_MJD, "%lu", &mjd);
     sscanf(RX_MPM, "%lu", &mpm);
     mpm += deltaT + 1;
     if( mpm > 86399999 ) {
     	mjd += 1;
          mpm -= 86400000;
    	}

     // Build the return message
     sprintf(buf, "%3s%3s%3s%9s%4i%6lu%9lu %1s%7s%s", \
     	"MCS", aspSubsystem, RX_TYPE, RX_REF, (strlen(TX_DATA)+8), \
          mjd, mpm, status, mib->index1.summary, TX_DATA);

	return 1;
}

#endif

