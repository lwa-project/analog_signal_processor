/*
 aspFunction.h - Header library to deal with the interaction with the ARX boards.
 
 Function Defined:
  * initASP - initalize ASP
  * setFEEPower - set the FEE power state for a certain antenna
  * setFilter - set the ARX filter
  * setAT1 - set the level of the first attenuator
  * setAT2 - set the level of the second attenuator
  * setATS - set the level of the split bandwidth mode attenuator
  * addToQueue - queue a command to run later
  * processQueue - monitor the queue and run the queued commands

$Rev$
$LastChangedBy$
$LastChangedDate$
*/

#include aspSPI.h
#include aspCommon.h


/*
 Structure to store pending commands in
*/
typedef struct {
	int status;
	char command[257];
	int setting1;
	int setting2;
	int device;
	int num;
} pendingCommand;


/*
 The actual command queue
*/
pendingCommand commandQueue[2048];


void initASP(void);
void setFEEPower(int, int, int);
void setFilter(int, int, int);
void setAT1(int, int , int);
void setAT2(int, int, int);
void setATS(int, int, int);
int addToQueue(char*, int, int, int, int)
void processQueue(void);


/*
 initASP - ready ASP for action
*/

void initASP(void) {
	int i;
	
	//SPI device initialization routine
	SPI_init_devices(nChP, SPI_cfg_normal);               		//out of sleep mode
	SPI_config_devices(nChP, SPI_cfg_output_P16_17_18_19);  	//set outputs
	SPI_config_devices(nChP, SPI_cfg_output_P20_21_22_23);  	//set outputs
	SPI_config_devices(nChP, SPI_cfg_output_P24_25_26_27);  	//set outputs
	SPI_config_devices(nChP, SPI_cfg_output_P28_29_30_31);  	//set outputs

	//Set default values (full attenuation, filters off, FEEs off)
	for(i=1; i<=8; i++) {
		setFEEPower(1, 0, i, nChP);
		setFEEPower(2, 0, i, nChP);
	}
	
	for(i=1; i<=8; i++) {
		setAT1(30, i, nChP);
		setAT2(30, i, nChP);
		setATS(30, i, nChP);
	}
	
	for(i=1; i<=8; i++) {
		setFilter(0, i, nChP);
	}

	//Initialize MIB entries
	strcpy(mib.index1.summary, " NORMAL");
	strcpy(mib.index1.info, "ASP booted, but has not been initialized");
	strcpy(mib.index1.lastlog, "No LASTLOG exists");
	strcpy(mib.index1.subsystem, aspSubsystem);
	strcpy(mib.index1.serialno, aspSerialNumber);
	strcpy(mib.index1.version, aspVersion);

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


/*
 setFEEPower - set the FEE for particular stand,pol (as a device, num pair)
*/

void setFEEPower(int channel, int power, int device, int num) {
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


/*
 setFilter - set the filter for particular stand (as a device, num pair)
*/

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


/*
 setAT1 - set the first attenuator for particular stand (as a device, num pair)
*/

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


/*
 setAT2 - set the second attenuator for particular stand (as a device, num pair)
*/

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


/*
 setATS - set the split bandwidth mode attenuator for particular stand (as a device, num pair)
*/

void setATS(int setting, int device, int num) {
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


/*
 addToQue - add a command (and all of the necessary information) to the 
                   command queue to be executed by ASP when possible
*/

int addToQueue(char *command, int setting1, int setting2, int device, int num) {
	auto int i;
	int accepted;
	
	for(i=0; i<CMD_QUEUE_SIZE; i++) {
		// If we've found an empty entry, use it
		if( commandQueue[i].status == -1 ) {
			commandQueue[i].status = 0
			
			strcpy(commandQueue[i].command, command);
			commandQueue[i].setting1 = setting1;
			commandQueue[i].setting2 = settgin2;
			commandQueue[i].device = device;
			commandQueue[i].num = num;
			
			commandQueue[i].status = 1;
			accepted = 1;
			
			printf("Adding command '%s' to queue at slot %i\n", commandQueue[i].command, i);
			
			break;
		}
	}
	
	return accepted;
}


/* 
 processQueue - deal with pending commands in the queue
*/

void processQueue(void) {
	auto int i, Stand, nChP;
	
	for(i=0; i<CMD_QUEUE_SIZE; i++) {
		// Look for useable entries
		if( commandQueue[i].status == 1 ) {
			Stand = commandQueue[i].device;
			nChP = commandQueue[i].num;
			
			printf("Processing command '%s' at slot %i, stand %i\n", commandQueue[i].command, i, Stand);
			
			if( !strcmp(commandQueue[i].command, "setFEEPower") ) {
				setFEEPower(commandQueue[i].setting1, commandQueue[i].setting2, Stand, nChP);
				if( commandQueue[i].setting1 == 1 ) {
					mib.index5.FEEPOL1PWR[Stand-1] = 1;
				} else {
					mib.index5.FEEPOL2PWR[Stand-1] = 1;
				}
				commandQueue[i].status = -1;
				
			} else if( !strcmp(commandQueue[i].command, "setFilter") ) {
				setFilter(commandQueue[i].setting1, Stand, nChP);
				mib.index3.FILTER[Stand-1] = commandQueue[i].setting1;
				commandQueue[i].status = -1;
				
			} else if( !strcmp(commandQueue[i].command, "setAT1") ) {
				setAT1(commandQueue[i].setting1, Stand, nChP);
				mib.index4.AT1[Stand-1] = commandQueue[i].setting1;
				commandQueue[i].status = -1;
				
			} else if( !strcmp(commandQueue[i].command, "setAT2") ) {
				setAT2(commandQueue[i].setting1, Stand, nChP);
				mib.index4.AT2[Stand-1] = commandQueue[i].setting1;
				commandQueue[i].status = -1;
				
			} else if( !strcmp(commandQueue[i].command, "setATS") ) {
				setATS(commandQueue[i].setting1, Stand, nChP);
				mib.index4.ATS[Stand-1] = commandQueue[i].setting1;
				commandQueue[i].status = -1;
				
			} else {
				printf("Unrecognized command '%s' in queue at slot %i, skipping\n", commandQueue[i].command, i);
				
				commandQueue[i] = -1
			}
		}
	}
}
