#ifndef __ASPFUNCTIONS_H
#define __ASPFUNCTIONS_H

/*
 aspFunction.h - Header library to deal with the interaction with the ARX boards.

 Function Defined:
  * initQueue - initalize the ASP command queue
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

#include aspMIB.h
#include aspSPI.h
#include aspCommon.h


/*
 Structure to store pending commands in
*/
typedef struct {
	int status;
	char command[32];
	int setting1;
	int setting2;
	int device;
	int num;
} pendingCommand;


/*
 Structure to store the actual command queue
*/
typedef struct {
	int nCommands;
     int maxCommands;
     pendingCommand queue[32];
} aspCommandQueue;


void initASP(aspMIB*);
void setFEEPower(int, int, int, int);
void setFilter(int, int, int);
void setAT1(int, int , int);
void setAT2(int, int, int);
void setATS(int, int, int);
int addToQueue(aspCommandQueue*, char*, int, int, int, int);
int processQueue(aspMIB*, aspCommandQueue*);


/*
 initQueue - initilize the command queue
*/
int initQueue(aspCommandQueue* commandQueue) {
	auto int i;

	commandQueue->nCommands = 0;
     commandQueue->maxCommands = CMD_QUEUE_SIZE;

     for(i=0; i<commandQueue->maxCommands; i++) {
     	commandQueue->queue[i].status = -1;
          commandQueue->queue[i].setting1 = 0;
          commandQueue->queue[i].setting2 = 0;
          commandQueue->queue[i].device = 0;
          commandQueue->queue[i].num = 0;
     }

     return 1;
}


/*
 initASP - ready ASP for action
*/

void initASP(aspMIB* mib) {
	int i;
     unsigned long int T1, T2;

     T1 = MS_TIMER;

	//SPI device initialization routine
	SPI_init_devices(mib->nChP, SPI_cfg_normal);               		//out of sleep mode
	SPI_config_devices(mib->nChP, SPI_cfg_output_P16_17_18_19);  	//set outputs
	SPI_config_devices(mib->nChP, SPI_cfg_output_P20_21_22_23);  	//set outputs
	SPI_config_devices(mib->nChP, SPI_cfg_output_P24_25_26_27);  	//set outputs
	SPI_config_devices(mib->nChP, SPI_cfg_output_P28_29_30_31);  	//set outputs

     /*
	//Set default values (full attenuation, filters off, FEEs off)
	for(i=1; i<=8; i++) {
		setFEEPower(1, 0, i, mib->nChP);
		setFEEPower(2, 0, i, mib->nChP);
	}

	for(i=1; i<=8; i++) {
		setAT1(30, i, mib->nChP);
		setAT2(30, i, mib->nChP);
		setATS(30, i, mib->nChP);
	}

	for(i=1; i<=8; i++) {
		setFilter(0, i, mib->nChP);
	}
     */

	//Initialize MIB entries
	strcpy(mib->index2.ARXSUPPLY, "OFF");
	strcpy(mib->index2.ARXSUPPLY_NO, " 1");
	strcpy(mib->index2.ARXPWRUNIT_1, "This is mock info about the ARX power supply");
	strcpy(mib->index2.ARXCURR, "      0");
	strcpy(mib->index2.FEESUPPLY, "OFF");
	strcpy(mib->index2.FEESUPPLY_NO, " 1");
	strcpy(mib->index2.FEEPWRUNIT_1, "This is mock info about the FEE power supply");
	strcpy(mib->index2.FEECURR, "      0");

	strcpy(mib->index6.TEMP_STATUS, "IN_RANGE");
	strcpy(mib->index6.TEMP_SENSE_NO, "  1");
	strcpy(mib->index6.SENSOR_NAME_1, "mock sensor name here");
	strcpy(mib->index6.SENSOR_DATA_1, "        25");

     // Finish up by setting the ASP state
     T2 = MS_TIMER;
     strcpy(mib->index1.summary, "NORMAL");
	sprintf(mib->index1.info, "ASP initialized in %.3f s", (T2-T1)/1000.0);
	strcpy(mib->index1.lastlog, "INI completed");

     // printf("ASP Initialized with %d ARX boards installed \n", mib->nBoards);
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

int addToQueue(aspCommandQueue* commandQueue, char *command, int setting1, int setting2, int device, int num) {
	auto int i;
	int accepted;

	for(i=0; i<commandQueue->maxCommands; i++) {
		// If we've found an empty entry, use it
		if( commandQueue->queue[i].status == -1 ) {
			commandQueue->queue[i].status = 0;

			strcpy(commandQueue->queue[i].command, command);
			commandQueue->queue[i].setting1 = setting1;
			commandQueue->queue[i].setting2 = setting2;
			commandQueue->queue[i].device = device;
			commandQueue->queue[i].num = num;

			commandQueue->queue[i].status = 1;
               commandQueue->nCommands += 1;
			accepted = 1;

			printf("Adding command '%s' to queue at slot %i\n", commandQueue->queue[i].command, i);

			break;
		}
	}

	return accepted;
}


/*
 processQueue - deal with pending commands in the queue
*/

int processQueue(aspMIB* mib, aspCommandQueue* commandQueue) {
	auto int i, j, Stand, nChP;

     for(i=0; i<commandQueue->maxCommands; i++) {
          if( commandQueue->nCommands == 0 ) {
     		break;
	    	}

		// Look for useable entries
		if( commandQueue->queue[i].status == 1 ) {
			Stand = commandQueue->queue[i].device;
			nChP = commandQueue->queue[i].num;

			printf("Processing command '%s' at slot %i, stand %i (%i of %i)\n", commandQueue->queue[i].command, i, Stand, 1, commandQueue->nCommands);

               if( !strcmp(commandQueue->queue[i].command, "initASP") ) {
               	initASP(mib);

                    commandQueue->queue[i].status = -1;
                    commandQueue->nCommands -= 1;

			} else if( !strcmp(commandQueue->queue[i].command, "setFEEPower") ) {
               	// Single stand
                    if( Stand != 0 ) {
                    	setFEEPower(commandQueue->queue[i].setting1, commandQueue->queue[i].setting2, Stand, nChP);
	                    if( commandQueue->queue[i].setting1 == 1 ) {
	                         mib->index5.FEEPOL1PWR[Stand-1] = 1;
	                    } else {
	                         mib->index5.FEEPOL2PWR[Stand-1] = 1;
	                    }

                   	// Stand "0" case
                    } else {
					for(j=1; j<=260; j++) {
                              setFEEPower(commandQueue->queue[i].setting1, commandQueue->queue[i].setting2, j, nChP);
                        		if( commandQueue->queue[i].setting1 == 1 ) {
	                         	mib->index5.FEEPOL1PWR[j-1] = 1;
	                         } else {
                              	mib->index5.FEEPOL2PWR[j-1] = 1;
	                         }
                         }
                    }

				commandQueue->queue[i].status = -1;
                    commandQueue->nCommands -= 1;

			} else if( !strcmp(commandQueue->queue[i].command, "setFilter") ) {
               	// Single Stand
               	if( Stand != 0 ) {
					setFilter(commandQueue->queue[i].setting1, Stand, nChP);
					mib->index3.FILTER[Stand-1] = commandQueue->queue[i].setting1;

                 	// Stand "0" case
                 	} else {
                    	for(j=1; j<=nStands; j++) {
                              setFilter(commandQueue->queue[i].setting1, j, nChP);
						mib->index3.FILTER[j-1] = commandQueue->queue[i].setting1;
                         }
                    }

				commandQueue->queue[i].status = -1;
                    commandQueue->nCommands -= 1;

			} else if( !strcmp(commandQueue->queue[i].command, "setAT1") ) {
               	// Single stand
                    if( Stand != 0 ) {
					setAT1(commandQueue->queue[i].setting1, Stand, nChP);
					mib->index4.AT1[Stand-1] = commandQueue->queue[i].setting1;

                 	// Stand "0" case
                    } else {
                    	for(j=1; j<=nStands; j++) {
                              setAT1(commandQueue->queue[i].setting1, j, nChP);
						mib->index4.AT1[j-1] = commandQueue->queue[i].setting1;
                         }
                    }

				commandQueue->queue[i].status = -1;
                    commandQueue->nCommands -= 1;

			} else if( !strcmp(commandQueue->queue[i].command, "setAT2") ) {
               	// Single stand
                    if( Stand != 0 ) {
					setAT2(commandQueue->queue[i].setting1, Stand, nChP);
					mib->index4.AT2[Stand-1] = commandQueue->queue[i].setting1;

                 	// Stand "0" case
                    } else {
                    	for(j=1; j<=nStands; j++) {
                              setAT2(commandQueue->queue[i].setting1, j, nChP);
						mib->index4.AT2[j-1] = commandQueue->queue[i].setting1;
                         }
                    }

				commandQueue->queue[i].status = -1;
                    commandQueue->nCommands -= 1;

			} else if( !strcmp(commandQueue->queue[i].command, "setATS") ) {
               	// Single stand
                    if( Stand != 0 ) {
					setATS(commandQueue->queue[i].setting1, Stand, nChP);
					mib->index4.ATS[Stand-1] = commandQueue->queue[i].setting1;

                    // Stand "0" case
                    } else {
                    	for(j=1; j<=nStands; j++) {
                              setATS(commandQueue->queue[i].setting1, j, nChP);
						mib->index4.ATS[j-1] = commandQueue->queue[i].setting1;
                         }
                    }

				commandQueue->queue[i].status = -1;
                    commandQueue->nCommands -= 1;

			} else {
				printf("Unrecognized command '%s' in queue at slot %i, skipping\n", commandQueue->queue[i].command, i);

				commandQueue->queue[i].status = -1;
                    commandQueue->nCommands -= 1;
			}
		}
	}

     return 1;
}

#endif

