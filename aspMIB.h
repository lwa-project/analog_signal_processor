#ifndef __ASPMIB_H
#define __ASPMIB_H

/* MIB Structure Header file */
/**********************************************
$Rev$
$LastChangedBy$
$LastChangedDate$

************************************************/

//Setup MIB as a structure
typedef struct {
	char summary[8];
	char info[257];
	char lastlog[257];
	char subsystem[4];
	char serialno[6];
	char version[257];
} mcsInfo;


typedef struct {
	char ARXSUPPLY[4];
	char ARXSUPPLY_NO[3];
	char ARXPWRUNIT_1[257];
	char ARXCURR[8];
	char FEESUPPLY[4];
	char FEESUPPLY_NO[3];
	char FEEPWRUNIT_1[257];
	char FEECURR[8];
} arxInfo;


typedef struct {
	int FILTER[260];
} filterInfo;

typedef struct {
	int AT1[260];
	int AT2[260];
	int ATS[260];
} attenInfo;


typedef struct {
	int FEEPOL1PWR[260];
	int FEEPOL2PWR[260];
} feeInfo;


typedef struct {
	char TEMP_STATUS[257];
	char TEMP_SENSE_NO[4];
	char SENSOR_NAME_1[257];
	char SENSOR_DATA_1[11];
} sensorInfo;


typedef struct {
	int init;           //initialization flag
     int nBoards;        //number of boards installed
     int nStands;        //number of stands
     int nChP;           //number of channel pairs (SPI devices)

     mcsInfo index1;
     arxInfo index2;
     filterInfo index3;
     attenInfo index4;
     feeInfo index5;
     sensorInfo index6;
} aspMIB;

#endif

