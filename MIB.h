/* MIB Structure Header file */
/**********************************************
$Rev$
$LastChangedBy$
$LastChangedDate$

************************************************/

//Setup MIB as a structure
typedef struct {
	struct {
		char summary[8];
		char info[257];
		char lastlog[257];
		char subsystem[4];
		char serialno[6];
		char version[257];
	} index1;
	
	struct {
		char ARXSUPPLY[4];
		char ARXSUPPLY_NO[3];
		char ARXPWRUNIT_1[257];
		char ARXCURR[8];
		char FEESUPPLY[4];
		char FEESUPPLY_NO[3];
		char FEEPWRUNIT_1[257];
		char FEECURR[8];
	} index2;
	
	struct {
		int FILTER[260][2];
	} index3;
	
	struct {
		int AT1[260][3];
		int AT2[260][3];
		int ATS[260][3];
	} index4;
	
	struct {
		int FEEPOL1PWR[260];
		int FEEPOL2PWR[260];
	} index5;
	
	struct {
		char TEMP_STATUS[257];
		char TEMP_SENSE_NO[4];
		char SENSOR_NAME_1[257];
		char SENSOR_DATA_1[11];
	} index6;
} aspMIB;