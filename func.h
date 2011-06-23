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

int receive_packet(void) {
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

	//Log info
	if(strcmp(RX_DATA, "LASTLOG")) {
		strcpy(mib.index1.lastlog, RX_MJD);
		strcat(mib.index1.lastlog, RX_MPM);
		strcat(mib.index1.lastlog, " ");
		strcat(mib.index1.lastlog, RX_TYPE);
		strcat(mib.index1.lastlog, " ");
		strcat(mib.index1.lastlog, RX_DATA);
		strcat(mib.index1.lastlog, " ");
	}

	if (init || !strcmp(RX_TYPE, "INI")){
		if (!strcmp(RX_TYPE, "PNG")){
			printf("Received a Ping \n");
			strcpy(TX_DATA, "A");
			strcat(TX_DATA, mib.index1.summary);
		} else if (!strcmp(RX_TYPE, "RPT")) {	//Report Command
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
					strcat(TX_DATA, mib.index3.FILTER[Stand-1]);
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
				if(0 < Stand <261){
					strcat(TX_DATA, mib.index4.AT1[Stand-1]);
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
				if(0 < Stand <261) {
					strcat(TX_DATA, mib.index4.AT2[Stand-1]);
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
				if(0 < Stand <261) {
					strcat(TX_DATA, mib.index4.ATS[Stand-1]);
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
				if(0 < Stand <261) {
					strcat(TX_DATA, mib.index5.FEEPOL1PWR[Stand-1]);
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
				if(0 < Stand <261) {
					strcat(TX_DATA, mib.index5.FEEPOL2PWR[Stand-1]);
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
		} else if(!strcmp(RX_TYPE, "INI")){      //Initialize Command
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
		} else if(!strcmp(RX_TYPE, "FIL")) {    //Filter Command
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
					strcpy(mib.index3.FILTER[Stand-1], "0");
					setFilter(3, Stand, nChP);
				} else if (nFset == 1) { //full BW
					strcpy(mib.index3.FILTER[Stand-1], "1");
					setFilter(1, Stand, nChP);
				} else if (nFset == 2) { //reduced BW
					strcpy(mib.index3.FILTER[Stand-1], "2");
					setFilter(2, Stand, nChP);
				} else if (nFset == 3) { //filter off
					strcpy(mib.index3.FILTER[Stand-1], "3");
					setFilter(0, Stand, nChP);
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
		} else if(!strcmp(RX_TYPE, "AT1")) {    //Attenuator 1 Command
			strcpy(sSTD, " ");
			strcpy(sATset, " ");
			printf("AT1 Command Received, Data -> %s \n", RX_DATA);
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

				strcpy(mib.index4.AT1[Stand-1], sATset);
				setAT1(2*nATset, Stand, nChP);
			} else {
				strcpy(TX_DATA, "R");
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " AT1 data field out of range");
			}
		} else if(!strcmp(RX_TYPE, "AT2")) {    //Attenuator 2 Command
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

				strcpy(mib.index4.AT2[Stand-1], sATset);
				setAT2(2*nATset, Stand, nChP);

			} else {
				strcpy(TX_DATA, "R");
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " AT2 data field out of range");
			}
		} else if(!strcmp(RX_TYPE, "ATS")) {    //Split BW Attenuator Command
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

				strcpy(mib.index4.ATS[Stand-1], sATset);
				setSplitAT(2*nATset, Stand, nChP);
			} else {
				strcpy(TX_DATA, "R");
				strcat(TX_DATA, mib.index1.summary);
				strcat(TX_DATA, " ATS data field out of range");
			}
		} else if(!strcmp(RX_TYPE, "FPW")) {    //FEE Power Command
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
						strcpy(mib.index5.FEEPOL1PWR[Stand-1], "ON ");
						setFEE(1, 1, Stand, nChP);
					} else if(!strcmp(sPWRset, "00")) {
						strcpy(mib.index5.FEEPOL1PWR[Stand-1], "OFF");
						setFEE(1, 0, Stand, nChP);
					} else {
						strcpy(TX_DATA, "R");
						strcat(TX_DATA, mib.index1.summary);
						strcat(TX_DATA, " FPW power setting out of range");
					}
				} else if (!strcmp(sPolSet, "2")) {	//Polarization 2
					if (!strcmp(sPWRset, "11")) {
						strcpy(mib.index5.FEEPOL2PWR[Stand-1], "ON ");
						setFEE(2, 1, Stand, nChP);
					} else if(!strcmp(sPWRset, "00")) {
						strcpy(mib.index5.FEEPOL2PWR[Stand-1], "OFF");
						setFEE(2, 0, Stand, nChP);
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
		} else if(!strcmp(RX_TYPE, "SHT")) {
			SPI_config_devices(nChP, SPI_cfg_shutdown);			//into sleep mode
			init = 0;
			strcpy(TX_DATA, "A");
			strcat(TX_DATA, mib.index1.summary);
		}

		/*
		else if(!strcmp(RX_TYPE, "RXP")){    //ARX Power Supply Command
		//do stuff
		}
		else if(!strcmp(RX_TYPE, "FEP")){    //FEE Power Supply Command
		//do stuff
		}
		*/

		else {   //Reject if command doesn't exist
			strcpy(TX_DATA, "R");
			strcat(TX_DATA, mib.index1.summary);
			strcat(TX_DATA,  " Command Invalid");
		}
	} else {	//Reject if not initialize
		strcpy(TX_DATA, "R");
		strcat(TX_DATA, mib.index1.summary);
		strcat(TX_DATA,  " Initialize the ASP first");
	}
	strcat(mib.index1.lastlog, TX_DATA);	//add TX_DATA to LASTLOG entry
	return 1;
}


int send_packet(void) {
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


void setSPIconst(void) {
	//SPI constants
	SPI_cfg_normal = 0x0104;
	SPI_cfg_shutdown = 0x0004;
	SPI_cfg_output_P16_17_18_19 = 0x550C;
	SPI_cfg_output_P20_21_22_23 = 0x550D;
	SPI_cfg_output_P24_25_26_27 = 0x550E;
	SPI_cfg_output_P28_29_30_31 = 0x550F;

	SPI_P16_on = 0x0130;
	SPI_P16_off = 0x0030;
	SPI_P17_on = 0x0131;
	SPI_P17_off = 0x0031;
	SPI_P18_on = 0x0132;
	SPI_P18_off = 0x0032;
	SPI_P19_on = 0x0133;
	SPI_P19_off = 0x0033;
	SPI_P20_on = 0x0134;
	SPI_P20_off = 0x0034;
	SPI_P21_on = 0x0135;
	SPI_P21_off = 0x0035;
	SPI_P22_on = 0x0136;
	SPI_P22_off = 0x0036;
	SPI_P23_on = 0x0137;
	SPI_P23_off = 0x0037;
	SPI_P24_on = 0x0138;
	SPI_P24_off = 0x0038;
	SPI_P25_on = 0x0139;
	SPI_P25_off = 0x0039;
	SPI_P26_on = 0x013A;
	SPI_P26_off = 0x003A;
	SPI_P27_on = 0x013B;
	SPI_P27_off = 0x003B;

	SPI_P28_on = 0x013C;
	SPI_P28_off = 0x003C;
	SPI_P29_on = 0x013D;
	SPI_P29_off = 0x003D;
	SPI_P30_on = 0x013E;
	SPI_P30_off = 0x003E;
	SPI_P31_on = 0x013F;
	SPI_P31_off = 0x003F;
}


void initMIB(void) {
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


void initASP(void) {
	//SPI device initialization routine
	SPI_init_devices(nChP, SPI_cfg_normal);               	//out of sleep mode
	SPI_config_devices(nChP, SPI_cfg_output_P16_17_18_19);  	//set outputs
	SPI_config_devices(nChP, SPI_cfg_output_P20_21_22_23);  	//set outputs
	SPI_config_devices(nChP, SPI_cfg_output_P24_25_26_27);  	//set outputs
	SPI_config_devices(nChP, SPI_cfg_output_P28_29_30_31);  	//set outputs

	//Set default values (full attenuation, filters off, FEEs off)
	setFEE(1, 0, 1, nChP);
	setFEE(2, 0, 1, nChP);
	setFEE(1, 0, 2, nChP);
	setFEE(2, 0, 2, nChP);
	setFEE(1, 0, 3, nChP);
	setFEE(2, 0, 3, nChP);
	setFEE(1, 0, 4, nChP);
	setFEE(2, 0, 4, nChP);
	setFEE(1, 0, 5, nChP);
	setFEE(2, 0, 5, nChP);
	setFEE(1, 0, 6, nChP);
	setFEE(2, 0, 6, nChP);
	setFEE(1, 0, 7, nChP);
	setFEE(2, 0, 7, nChP);
	setFEE(1, 0, 8, nChP);
	setFEE(2, 0, 8, nChP);
	setAT1(30, 1, nChP);
	setAT2(30, 1, nChP);
	setSplitAT(30, 1, nChP);
	setAT1(30, 2, nChP);
	setAT2(30, 2, nChP);
	setSplitAT(30, 2, nChP);
	setAT1(30, 3, nChP);
	setAT2(30, 3, nChP);
	setSplitAT(30, 3, nChP);
	setAT1(30, 4, nChP);
	setAT2(30, 4, nChP);
	setSplitAT(30, 4, nChP);
	setAT1(30, 5, nChP);
	setAT2(30, 5, nChP);
	setSplitAT(30, 5, nChP);
	setAT1(30, 6, nChP);
	setAT2(30, 6, nChP);
	setSplitAT(30, 6, nChP);
	setAT1(30, 7, nChP);
	setAT2(30, 7, nChP);
	setSplitAT(30, 7, nChP);
	setAT1(30, 8, nChP);
	setAT2(30, 8, nChP);
	setSplitAT(30, 8, nChP);
	setFilter(0, 1, nChP);
	setFilter(0, 2, nChP);
	setFilter(0, 3, nChP);
	setFilter(0, 4, nChP);
	setFilter(0, 5, nChP);
	setFilter(0, 6, nChP);
	setFilter(0, 7, nChP);
	setFilter(0, 8, nChP);

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


	strcpy(mib.index6.TEMP_STATUS, "IN_RANGE");
	strcpy(mib.index6.TEMP_SENSE_NO, "  1");
	strcpy(mib.index6.SENSOR_NAME_1, "mock sensor name");
	strcpy(mib.index6.SENSOR_DATA_1, "        25");
}


void SPI_init_devices(int num, int Config){
	int i, SPI_read;
	
	for (i = 0; i < 2*num; i++) {
		BitWrPortI(PBDR, &PBDRShadow, 0, 7);           	// chip select low
		SPIWrRd(&Config, &SPI_read, 2);        	  		// setup for normal operation
		BitWrPortI(PBDR, &PBDRShadow, 1, 7);         	// chip select high
	}
}


void SPI_config_devices(int num, int Config){
	int i;
	
	for (i = 0; i < num; i++) {
		SPI_Send(Config, i+1, num);
	}
}


void setFilter(int setting, int device, int num) {
	if (setting == 0) {
		// Set Filters OFF
		SPI_Send(SPI_P19_on, device, num);
		SPI_Send(SPI_P18_on, device, num);
	} else if (setting == 1) {
		// Set Filter to Full Bandwidth
		SPI_Send(SPI_P19_off, device, num);
		SPI_Send(SPI_P18_on, device, num);
	} else if (setting == 2) {
		// Set Filter to Reduced Bandwidth
		SPI_Send(SPI_P19_on, device, num);
		SPI_Send(SPI_P18_off, device, num);
	} else if (setting == 3) {
		// Set Filter to Split Bandwidth
		SPI_Send(SPI_P19_off, device, num);
		SPI_Send(SPI_P18_off, device, num);
	}
}


void setFEE(int channel, int power, int device, int num) {
	if (power == 1){
		if (channel == 1){
			SPI_Send(SPI_P17_on, device, num);
		} else if (channel == 2) {
			SPI_Send(SPI_P16_on, device, num);
		}
	} else if (power == 0) {
		if (channel == 1) {
			SPI_Send(SPI_P17_off, device, num);
		} else if (channel == 2) {
			SPI_Send(SPI_P16_off, device, num);
		}
	}
}


void setSplitAT(int setting, int device, int num) {
	// Set all to Zero
	SPI_Send(SPI_P31_off, device, num); // 16dB
	SPI_Send(SPI_P28_off, device, num); // 8dB
	SPI_Send(SPI_P29_off, device, num); // 4dB
	SPI_Send(SPI_P30_off, device, num); // 2dB

	if (setting >= 16) {
		SPI_Send(SPI_P31_on, device, num); // 16dB
		setting = setting - 16;
	}
	if (setting >= 8) {
		SPI_Send(SPI_P28_on, device, num); // 8dB
		setting = setting - 8;
	}
	if (setting >= 4) {
		SPI_Send(SPI_P29_on, device, num); // 4dB
		setting = setting - 4;
	}
	if (setting >= 2) {
		SPI_Send(SPI_P30_on, device, num); // 2dB
		setting = setting - 2;
	}
}


void setAT1(int setting, int device, int num) {
	// Set all to Zero
	SPI_Send(SPI_P27_off, device, num); // 16dB
	SPI_Send(SPI_P24_off, device, num); // 8dB
	SPI_Send(SPI_P25_off, device, num); // 4dB
	SPI_Send(SPI_P26_off, device, num); // 2dB

	if (setting >= 16) {
		SPI_Send(SPI_P27_on, device, num); // 16dB
		setting = setting - 16;
	}
	if (setting >= 8) {
		SPI_Send(SPI_P24_on, device, num); // 8dB
		setting = setting - 8;
	}
	if (setting >= 4) {
		SPI_Send(SPI_P25_on, device, num); // 4dB
		setting = setting - 4;
	}
	if (setting >= 2) {
		SPI_Send(SPI_P26_on, device, num); // 2dB
		setting = setting - 2;
	}
}


void setAT2(int setting, int device, int num) {
	// Set all to Zero
	SPI_Send(SPI_P23_off, device, num); // 16dB
	SPI_Send(SPI_P21_off, device, num); // 8dB
	SPI_Send(SPI_P20_off, device, num); // 4dB
	SPI_Send(SPI_P22_off, device, num); // 2dB

	if (setting >= 16 ) {
		SPI_Send(SPI_P23_on, device, num); // 16dB
		setting = setting - 16;
	}
	if (setting >= 8) {
		SPI_Send(SPI_P21_on, device, num); // 8dB
		setting = setting - 8;
	}
	if (setting >= 4) {
		SPI_Send(SPI_P20_on, device, num); // 4dB
		setting = setting - 4;
	}
	if (setting >= 2) {
		SPI_Send(SPI_P22_on, device, num); // 2dB
		setting = setting - 2;
	}
}


void SPI_Send(unsigned data, int device, int num) {
	int SPI_read, SPI_NoOp, i;
	SPI_NoOp = 0x0000;

	BitWrPortI(PBDR, &PBDRShadow, 0, 7);            // chip select low
	//printf("device is %02X\n", device);
	for (i = num; i > 0; i--) {
		if (i == device) {
			SPIWrRd(&data, &SPI_read, 2);             // write & read SPI
			//printf("sent: %04X\n", data);
		} else {
			SPIWrRd(&SPI_NoOp, &SPI_read, 2);               // write & read SPI
			//printf("sent: %04X\n", SPI_NoOp);
		}
	}
	BitWrPortI(PBDR, &PBDRShadow, 1, 7);            // chip select high
}
