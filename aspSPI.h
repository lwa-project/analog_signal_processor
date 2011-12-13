#ifndef __ASPSPI_H
#define __ASPSPI_H

/*
 aspSPI.h - Header library for interfacing wiht the SPI bus.

 Function defined:
  * setSPIconst - some SPI mumbo-jumbo
  * SPI_init_devices - initalize SPI devices
  * SPI_config_devices - configure SPI devices
  * SPI_Send - send a SPI command out to a particular device
  * SPI_Send_All - send a SPI command to all devices

$Rev$
$LastChangedBy$
$LastChangedDate$
*/

#define SPI_SER_C
#define SPI_RX_PORT SPI_RX_PE
#define SPI_CLK_DIVISOR 100

#use "spi.lib"


//SPI definition
int SPI_cfg_normal;
int SPI_cfg_shutdown;
int SPI_cfg_output_P16_17_18_19;
int SPI_cfg_output_P20_21_22_23;
int SPI_cfg_output_P24_25_26_27;
int SPI_cfg_output_P28_29_30_31;
int SPI_P16_on;
int SPI_P16_off;
int SPI_P17_on;
int SPI_P17_off;
int SPI_P18_on;
int SPI_P18_off;
int SPI_P19_on;
int SPI_P19_off;
int SPI_P20_on;
int SPI_P20_off;
int SPI_P21_on;
int SPI_P21_off;
int SPI_P22_on;
int SPI_P22_off;
int SPI_P23_on;
int SPI_P23_off;
int SPI_P24_on;
int SPI_P24_off;
int SPI_P25_on;
int SPI_P25_off;
int SPI_P26_on;
int SPI_P26_off;
int SPI_P27_on;
int SPI_P27_off;
int SPI_P28_on;
int SPI_P28_off;
int SPI_P29_on;
int SPI_P29_off;
int SPI_P30_on;
int SPI_P30_off;
int SPI_P31_on;
int SPI_P31_off;

void setSPIconst(void);
void SPI_init_devices(int, int);
void SPI_config_devices(int, int);
void SPI_Send(unsigned, int, int);
void SPI_Send_All(unsigned, int);


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

	SPI_Send_All(Config, num);
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


void SPI_Send_All(unsigned data, int num) {
	int SPI_read, i;

	BitWrPortI(PBDR, &PBDRShadow, 0, 7);            // chip select low
	//printf("device is %02X\n", device);
	for (i = num; i > 0; i--) {
		SPIWrRd(&data, &SPI_read, 2);             // write & read SPI
		//printf("sent: %04X\n", data);
	}
	BitWrPortI(PBDR, &PBDRShadow, 1, 7);            // chip select high
}
#endif

