/*
  aspMCSInterface.h - Header library to deal with communicating with MCS
                      and handling commands, reporting on MIB entries, etc.
                      
  Functions defined:
   * initMIB - create the default MIB database
   * receiveCommand - process in incoming MCS packet
   * sendResponse - respond to a MCS packet
*/

#include MIB.h
#include aspCommon.h
#include aspFunctions.h


//buffers for received data
char buf[64];
char RX_DEST[4];
char RX_SNDR[4];
char RX_TYPE[4];
char RX_REF[10];
char RX_DLEN[5];
char RX_MJD[7];
char RX_MPM[10];
char RX_DATA[257];
char TX_DATA[257];


int init;		//initialization flag
int nBoards;	//number of boards installed
int nChP;		//number of channel pairs (SPI devices)


// Function definitions
void initMIB(void);
int recieveCommand(void);
int sendResponse(void);


/*
 initMIB - Function to initialize a MIB structure with default values
*/

void initMIB(void) {
	int i;
	
	//Initialize MIB entries
	strcpy(mib.index1.summary, " NORMAL");
	strcpy(mib.index1.info, "ASP booted, but has not been initialized");
	strcpy(mib.index1.lastlog, "No LASTLOG exists");
	strcpy(mib.index1.subsystem, "ASP");
	strcpy(mib.index1.serialno, "ASP01");
	strcpy(mib.index1.version, "preAlpha");

	strcpy(mib.index2.ARXSUPPLY, "OFF");
	strcpy(mib.index2.ARXSUPPLY_NO, " 1");
	strcpy(mib.index2.ARXPWRUNIT_1, "This is mock info about the ARX power supply");
	strcpy(mib.index2.ARXCURR, "      0");
	strcpy(mib.index2.FEESUPPLY, "OFF");
	strcpy(mib.index2.FEESUPPLY_NO, " 1");
	strcpy(mib.index2.FEEPWRUNIT_1, "This is mock info about the FEE power supply");
	strcpy(mib.index2.FEECURR, "      0");

	for(i=1; i<=260; i++) {
		strcpy(mib.index3.FILTER[i-1], "3");
		strcpy(mib.index4.AT1[i-1], "15");
		strcpy(mib.index4.AT2[i-1], "15");
		strcpy(mib.index4.ATS[i-1], "15");
		strcpy(mib.index5.FEEPOL1PWR[i-1], "OFF");
		strcpy(mib.index5.FEEPOL2PWR[i-1], "OFF");
	}

	strcpy(mib.index6.TEMP_STATUS, "IN_RANGE");
	strcpy(mib.index6.TEMP_SENSE_NO, "  1");
	strcpy(mib.index6.SENSOR_NAME_1, "mock sensor name");
	strcpy(mib.index6.SENSOR_DATA_1, "        25");
}


/*
 recieveCommand - deal with incoming MCS packets
*/

int receiveCommand(void) {
	int tstart_DEST, tstart_SNDR, tstart_TYPE, tstart_REF, tstart_DLEN, tstart_MJD, tstart_MPM, tstart_DATA;
	int tlength_DEST, tlength_SNDR, tlength_TYPE, tlength_REF, tlength_DLEN, tlength_MJD, tlength_MPM, tlength_DATA;
	int i;
	char sSTD[4];
	char sFset[3];
	int nFset;
	char sATset[3];
	int nATset;
	char sPolSet[2];
	char sPWRset[3];
	char sStand[3];
	int Stand;

	#GLOBAL_INIT
	{
		memset(buf, 0, sizeof(buf));
	}

	//Define packet message index & length
	tstart_DEST = 0;
	tlength_DEST = 3;
	tstart_SNDR = 3;
	tlength_SNDR = 3;
	tstart_TYPE = 6;
	tlength_TYPE = 3;
	tstart_REF = 9;
	tlength_REF = 9;
	tstart_DLEN = 18;
	tlength_DLEN = 4;
	tstart_MJD = 22;
	tlength_MJD = 6;
	tstart_MPM = 28;
	tlength_MPM = 9;
	tstart_DATA = 38;

	strcpy(buf, " ");		//Initialize the buffer

	/* receive the packet */
	if (-1 == udp_recv(&sock, buf, sizeof(buf))) {
		/* no packet read. return */
		return 0;
	}

	printf("RX -> %s\n",buf);

	//cycle through the message and strip off data
	strcpy(RX_DEST, " ");
	strcpy(RX_SNDR, " ");
	strcpy(RX_TYPE, " ");
	strcpy(RX_REF, " ");
	strcpy(RX_DLEN, " ");
	strcpy(RX_MJD, " ");
	strcpy(RX_MPM, " ");

	for(i = 0; i < tlength_DEST; i++) {
		RX_DEST[i] = buf[tstart_DEST+i];	//get DESTINATION
	}
	RX_DEST[i] = '\0';
	
	for(i = 0; i < tlength_SNDR; i++) {
		RX_SNDR[i] = buf[tstart_SNDR+i];	//get SENDER
	}
	RX_SNDR[i] = '\0';
	
	for(i = 0; i < tlength_TYPE; i++) {
		RX_TYPE[i] = buf[tstart_TYPE+i];	//get TYPE
	}
	RX_TYPE[i] = '\0';
	
	for(i = 0; i < tlength_REF; i++) {
		RX_REF[i] = buf[tstart_REF+i];	//get REFERENCE
	}
	RX_REF[i] = '\0';
	
	for(i = 0; i < tlength_DLEN; i++) {
		RX_DLEN[i] = buf[tstart_DLEN+i];	//get DATALEN
	}
	RX_DLEN[i] = '\0';
	
	for(i = 0; i < tlength_MJD; i++) {
		RX_MJD[i] = buf[tstart_MJD+i];	//get MJD
	}
	RX_MJD[i] = '\0';
	
	for(i = 0; i < tlength_MPM; i++) {
		RX_MPM[i] = buf[tstart_MPM+i];	//get MPM
	}
	RX_MPM[i] = '\0';

	//Get the data portion of the message
	tlength_DATA = strtol(RX_DLEN, NULL, 10);
	if (tlength_DATA > 0) {
		strcpy(RX_DATA, " ");
		for(i = 0; i < tlength_DATA; i++) {
			RX_DATA[i] = buf[tstart_DATA+i];	//get DATA
		}
		RX_DATA[i] = '\0';
	}

	// PNG
	if( !strcmp(RX_TYPE, "PNG") ) {
		printf("Received a Ping \n");
		strcpy(TX_DATA, "A");
		strcat(TX_DATA, mib.index1.summary);
		
	// RPT
	} else if( !strcmp(RX_TYPE, "RPT") ) {
		strcpy(TX_DATA, "A");     //default is Accepted
		strcat(TX_DATA, mib.index1.summary);
		strcat(TX_DATA, " ");

		printf("Reporting on MIB Entry -> %s \n", RX_DATA);

		if (!strcmp(RX_DATA, "SUMMARY"))
			strcat(TX_DATA, mib.index1.summary);      //add the summary entry to data
		else if (!strcmp(RX_DATA, "INFO"))
			strcat(TX_DATA, mib.index1.info);
		else if (!strcmp(RX_DATA, "LASTLOG"))
			strcat(TX_DATA, mib.index1.lastlog);
		else if (!strcmp(RX_DATA, "SUBSYSTEM"))
			strcat(TX_DATA, mib.index1.subsystem);
		else if (!strcmp(RX_DATA, "SERIALNO"))
			strcat(TX_DATA, mib.index1.serialno);
		else if (!strcmp(RX_DATA, "VERSION"))
			strcat(TX_DATA, mib.index1.version);
		else if (!strcmp(RX_DATA, "ARXSUPPLY"))
			strcat(TX_DATA, mib.index2.ARXSUPPLY);
		else if (!strcmp(RX_DATA, "ARXSUPPLY-NO"))
			strcat(TX_DATA, mib.index2.ARXSUPPLY_NO);
		else if (!strcmp(RX_DATA, "ARXPWRUNIT_1"))
			strcat(TX_DATA, mib.index2.ARXPWRUNIT_1);
		else if (!strcmp(RX_DATA, "ARXCURR"))
			strcat(TX_DATA, mib.index2.ARXCURR);
		else if (!strcmp(RX_DATA, "FEESUPPLY"))
			strcat(TX_DATA, mib.index2.FEESUPPLY);
		else if (!strcmp(RX_DATA, "FEESUPPLY_NO"))
			strcat(TX_DATA, mib.index2.FEESUPPLY_NO);
		else if (!strcmp(RX_DATA, "FEEPWRUNIT_1"))
			strcat(TX_DATA, mib.index2.FEEPWRUNIT_1);
		else if (!strcmp(RX_DATA, "FEECURR"))
			strcat(TX_DATA, mib.index2.FEECURR);
		//RPT FILTER
		else if (strstr(RX_DATA, "FILTER") != 0) {
			for(i = 0; i < 3; i++) {
				sStand[i] = RX_DATA[7+i];
			}
			Stand = atoi(sStand);
			if(0 < Stand < 261) {
				if( mib.index3.FILTER[Stand-1] == 0 ) {
					sprintf(TX_DATA, "%s %02i", TX_DATA, 3);
				} else if( mib.index3.FILTER[Stand-1] == 0 ) {
					sprintf(TX_DATA, "%s %02i", TX_DATA, 0);
				} else {
					sprintf(TX_DATA, "%s %02i", TX_DATA, mib.index3.FILTER[Stand-1]);
				}
			} else {
				strcpy(TX_DATA, "R");     //Rejected, no MIB entry
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " Stand number out-of-range");
			}
		}
		//RPT AT1
		else if (strstr(RX_DATA, "AT1") != 0) {
			for(i = 0; i < 3; i++) {
				sStand[i] = RX_DATA[4+i];
			}
			Stand = atoi(sStand);
			if(0 < Stand < 261){
				sprintf(TX_DATA, "%s %02i", TX_DATA, mib.index4.AT1[Stand-1]);
			} else {
				strcpy(TX_DATA, "R");     //Rejected, no MIB entry
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " Stand number out-of-range");
			}
		}
		//RPT AT2
		else if (strstr(RX_DATA, "AT2") != 0) {
			for(i = 0; i < 3; i++) {
				sStand[i] = RX_DATA[4+i];
			}
			Stand = atoi(sStand);
			if(0 < Stand < 261) {
				sprintf(TX_DATA, "%s %02i", TX_DATA, mib.index4.AT2[Stand-1]);
			} else {
				strcpy(TX_DATA, "R");     //Rejected, no MIB entry
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " Stand number out-of-range");
			}
		}
		//RPT AT Split
		else if (strstr(RX_DATA, "ATSPLIT") != 0) {
			for(i = 0; i < 3; i++) {
				sStand[i] = RX_DATA[8+i];
			}
			Stand = atoi(sStand);
			if(0 < Stand < 261) {
				sprintf(TX_DATA, "%s %02i", TX_DATA, mib.index4.ATS[Stand-1]);
			} else {
				strcpy(TX_DATA, "R");     //Rejected, no MIB entry
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " Stand number out-of-range");
			}
		}
		//RPT FEE Pol 1 Pwr
		else if (strstr(RX_DATA, "FEEPOL1PWR") != 0) {
			for(i = 0; i < 3; i++){
				sStand[i] = RX_DATA[11+i];
			}
			Stand = atoi(sStand);
			if(0 < Stand < 261) {
				if( mib.index5.FEEPOL1PWR[Stand-1] ) {
					sprintf(TX_DATA, "%s ON ", TX_DATA);
				} else {
					sprintf(TX_DATA, "%s OFF", TX_DATA);
				}
			} else {
				strcpy(TX_DATA, "R");     //Rejected, no MIB entry
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " Stand number out-of-range");
			}
		}
		//RPT FEE Pol 2 Pwr
		else if (strstr(RX_DATA, "FEEPOL2PWR") != 0) {
			for(i = 0; i < 3; i++){
				sStand[i] = RX_DATA[11+i];
			}
			Stand = atoi(sStand);
			if(0 < Stand < 261) {
				if( mib.index5.FEEPOL2PWR[Stand-1] ) {
					sprintf(TX_DATA, "%s ON ", TX_DATA);
				} else {
					sprintf(TX_DATA, "%s OFF", TX_DATA);
				}
			} else {
				strcpy(TX_DATA, "R");     //Rejected, no MIB entry
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " Stand number out-of-range");
			}
		}
		else if (!strcmp(RX_DATA, "TEMP-STATUS"))
			strcat(TX_DATA, mib.index6.TEMP_STATUS);
		else if (!strcmp(RX_DATA, "TEMP-SENSE-NO"))
			strcat(TX_DATA, mib.index6.TEMP_SENSE_NO);
		else if (!strcmp(RX_DATA, "SENSOR-NAME-1"))
			strcat(TX_DATA, mib.index6.SENSOR_NAME_1);
		else if (!strcmp(RX_DATA, "SENSOR-DATA-1"))
			strcat(TX_DATA, mib.index6.SENSOR_DATA_1);
		else {
			strcpy(TX_DATA, "R");     //Rejected, no MIB entry
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA, " MIB Entry Invalid");
		}
	
	/*
	 Commands down here
	*/
	
	// INI command
	} else if( !strcmp(RX_TYPE, "INI") ) {
		if (init) {	//ASP already initialized
			strcpy(TX_DATA, "R");
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA, " ASP already initialized (issue SHT, then INI to reinitialize)");
		} else {
			nBoards = strtol(RX_DATA, NULL, 10);
			if(0 < nBoards && nBoards < 34) {  //Do it
				nChP = 8*nBoards;
				init = 1;
				strcpy(TX_DATA, "A");
				strcat(TX_DATA, mib.index1.summary);
				initASP();		//Initialize all settings
				printf("ASP Initialized with %d ARX boards installed \n", nBoards);
			} else {
				strcpy(TX_DATA, "R");
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " ARX boards installed out of range");
			}
		}
	
	// FIL command
	} else if( !strcmp(RX_TYPE, "FIL") ) {
		if (!init) {
			strcpy(TX_DATA, "R");
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA, " ASP needs to be initialzed");
		} else {
			strcpy(sSTD, " ");
			strcpy(sFset, " ");
			printf("Filter Command Received, Data -> %s \n", RX_DATA);
			
			if (strlen(RX_DATA) == 5) {
				strcpy(TX_DATA, "A");     //default is Accepted
				strcat(TX_DATA, mib.index1.summary);
				for (i = 0; i <3; i++) {
					sSTD[i] = RX_DATA[i];
				}
				sSTD[i] = '\0';		//might be needed elsewhere?
				for (i = 3; i < 5; i++) {
					sFset[i-3] = RX_DATA[i];
				}
				sFset[i-3] = '\0';
				Stand = atoi(sSTD);
				nFset = atoi(sFset);

				if(nFset == 0) { //split BW
					addToQueue("setFilter", 3, Stand, nChP);
				} else if (nFset == 1) { //full BW
					addToQueue("setFilter", 1, Stand, nChP);
				} else if (nFset == 2) { //reduced BW
					addToQueue("setFilter", 2, Stand, nChP);
				} else if (nFset == 3) { //filter off
					addToQueue("setFilter", 0, Stand, nChP);
				} else {	//Stand Number out of range
					strcpy(TX_DATA, "R");
					strcat(TX_DATA, mib.index1.summary);
					strcat(TX_DATA, " Filter setting out-of-range");
				}
			} else {
				strcpy(TX_DATA, "R");
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " FIL data field out of range");
			}
		}
		
	// AT1 command
	} else if(!strcmp(RX_TYPE, "AT1")) {
		if (!init) {
			strcpy(TX_DATA, "R");
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA, " ASP needs to be initialzed");
		} else {
			strcpy(sSTD, " ");
			strcpy(sATset, " ");
			printf("AT1 Command Received, Data -> %s \n", RX_DATA);
			if (strlen(RX_DATA) == 5) {
				strcpy(TX_DATA, "A");     //default is Accepted
				strcat(TX_DATA, mib.index1.summary);
				for (i=0; i<3; i++) {
					sSTD[i] = RX_DATA[i];
				}
				for (i=3; i < 5; i++) {
					sATset[i-3] = RX_DATA[i];
				}
				sATset[i-3] = '\0';

				Stand = atoi(sSTD);
				nATset = atoi(sATset);

				addToQueue("setAT1", 2*nATset, Stand, nChP);
			} else {
				strcpy(TX_DATA, "R");
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " AT1 data field out of range");
			}
		}
	
	// AT2 command
	} else if(!strcmp(RX_TYPE, "AT2")) {
		if (!init) {
			strcpy(TX_DATA, "R");
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA, " ASP needs to be initialzed");
		} else {
			strcpy(sSTD, " ");
			strcpy(sATset, " ");
			printf("AT2 Command Received, Data -> %s \n", RX_DATA);
			if (strlen(RX_DATA) == 5) {
				strcpy(TX_DATA, "A");     //default is Accepted
				strcat(TX_DATA, mib.index1.summary);
				for (i = 0; i <3; i++) {
					sSTD[i] = RX_DATA[i];
				}
				for (i = 3; i < 5; i++) {
					sATset[i-3] = RX_DATA[i];
				}
				sATset[i-3] = '\0';

				Stand = atoi(sSTD);
				nATset = atoi(sATset);

				addToQueue("setAT2", 2*nATset, Stand, nChP);
			} else {
				strcpy(TX_DATA, "R");
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " AT2 data field out of range");
			}
		}
	
	// AT2 command
	} else if(!strcmp(RX_TYPE, "ATS")) {
		if (!init) {
			strcpy(TX_DATA, "R");
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA, " ASP needs to be initialzed");
		} else {
			strcpy(sSTD, " ");
			strcpy(sATset, " ");
			printf("ATS Command Received, Data -> %s \n", RX_DATA);
			if (strlen(RX_DATA) == 5) {
				strcpy(TX_DATA, "A");     //default is Accepted
				strcat(TX_DATA, mib.index1.summary);
				for (i = 0; i <3; i++) {
					sSTD[i] = RX_DATA[i];
				}
				for (i = 3; i < 5; i++) {
					sATset[i-3] = RX_DATA[i];
				}
				sATset[i-3] = '\0';

				Stand = atoi(sSTD);
				nATset = atoi(sATset);

				addToQueue("setATS", 2*nATset, Stand, nChP);
			} else {
				strcpy(TX_DATA, "R");
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " ATS data field out of range");
			}
		}
		
	// FPW command
	} else if(!strcmp(RX_TYPE, "FPW")) {
		if (!init) {
			strcpy(TX_DATA, "R");
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA, " ASP needs to be initialzed");
		} else {
			strcpy(sSTD, " ");
			strcpy(sPolSet, " ");
			strcpy(sPWRset, " ");
			printf("FPW Command Received, Data -> %s \n", RX_DATA);
			if (strlen(RX_DATA) == 6) {
				strcpy(TX_DATA, "A");     //default is Accepted
				strcat(TX_DATA, mib.index1.summary);
				for (i = 0; i <3; i++) {
					sSTD[i] = RX_DATA[i];
				}

				sPolSet[0] = RX_DATA[3];

				for (i = 4; i < 6; i++) {
					sPWRset[i-4] = RX_DATA[i];
				}
				sPWRset[i-4] = '\0';

				Stand = atoi(sSTD);
				if (!strcmp(sPolSet, "1")) {	//Polarization 1
					if (!strcmp(sPWRset, "11")) {
						addToQueue("setFEEPower", 1, 1, Stand, nChP);
					} else if(!strcmp(sPWRset, "00")) {
						addToQueue("setFEEPower", 1, 0, Stand, nChP);
					} else {
						strcpy(TX_DATA, "R");
						strcat(TX_DATA, mib.index1.summary);
						strcat(TX_DATA, " FPW power setting out of range");
					}
				} else if (!strcmp(sPolSet, "2")) {	//Polarization 2
					if (!strcmp(sPWRset, "11")) {
						addToQueue("setFEEPower", 2, 1, Stand, nChP);
					} else if(!strcmp(sPWRset, "00")) {
						addToQueue("setFEEPower", 2, 0, Stand, nChP);
					} else {
						strcpy(TX_DATA, "R");
						strcat(TX_DATA, mib.index1.summary);
						strcat(TX_DATA, " FPW power setting out of range");
					}
				} else { 	//FPW polarization out of range
					strcpy(TX_DATA, "R");
					strcat(TX_DATA, mib.index1.summary);
					strcat(TX_DATA, " FPW polarization out of range");
				}
			}
		}
		
	// SHT command
	} else if(!strcmp(RX_TYPE, "SHT")) {
		if (!init) {
			strcpy(TX_DATA, "R");
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA, " ASP needs to be initialzed");
		} else {
			SPI_config_devices(nChP, SPI_cfg_shutdown);			//into sleep mode
			init = 0;
			strcpy(TX_DATA, "A");
			strcat(TX_DATA, mib.index1.summary);
		}
		
	// Reject unknown commands
	} else {
			strcpy(TX_DATA, "R");
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA,  " Command Invalid");
			
	strcat(mib.index1.lastlog, TX_DATA);	//add TX_DATA to LASTLOG entry
	return 1;
}


/*
 sendResponse - reply to a MCS packet with another packet
*/

int sendResponse(void) {
	char buf[128];
	char buf_MPM[16];
	char buf_DataLength[16];
	char fTX_MPM[10];
	char fTX_DataLength[5];

	auto int length, retval, i, deltaMPM, deltaDataLength;
	long nMPM, nDataLength;

	strcpy(buf, "MCSASP");	//Always send back to MCS
	strcat(buf, RX_TYPE);   //Use the same type as last sent
	strcat(buf, RX_REF);    //Use the same reference last sent

	//Get length of data and pack it into a string
	nDataLength = strlen(TX_DATA);
	ltoa(nDataLength, buf_DataLength);
	strcpy(fTX_DataLength, "    ");
	deltaDataLength = 4 - strlen(buf_DataLength);
	for(i = 3; i >= deltaDataLength; i--) {
		fTX_DataLength[i] = buf_DataLength[i-deltaDataLength];
	}
	fTX_DataLength[4] = '\0';
	strcat(buf, fTX_DataLength);

	strcat(buf, RX_MJD); //MJD should always be the same

	//add 1 msec to MPM field and pack it back up into a string
	nMPM = strtol(RX_MPM, NULL, 10);
	nMPM = nMPM + 1;
	ltoa(nMPM, buf_MPM);
	strcpy(fTX_MPM, "         ");
	deltaMPM = 9 - strlen(buf_MPM);
	for (i = 8; i >= deltaMPM; i--) {
		fTX_MPM[i] = buf_MPM[i-deltaMPM];
	}

	strcat(buf, fTX_MPM);	//MPM field
	strcat(buf, " "); 		//always a space before the data
	strcat(buf, TX_DATA);   //DATA field

	length = strlen(buf) + 1;

	/* send the packet */
	retval = udp_send(&sock, buf, length);
	if (retval < 0) {
		printf("Error sending datagram!  Closing and reopening socket...\n");
		sock_close(&sock);
		if(!udp_open(&sock, LOCAL_PORT, resolve(REMOTE_IP), REMOTE_PORT, NULL)) {
			printf("udp_open failed!\n");
			exit(0);
		}
	} else {
		printf("TX -> %s\n", buf);
	}

	tcp_tick(NULL);
	return 1;
}
