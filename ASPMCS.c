/* ASP MCS software */
/**********************************************
Alpha release of ASP-MCS firmware, 6/12/10
author:  Joe Craig, University of New Mexico

Notes:
Multiple (daisy-chained) ARX boards supported (up to 33)
This release supports the RPT command with individual MIB entries only (no branching)
Real-time clock not yet implemented (MJD is always what MCS sends, MPM is what MCS sends + one msec)
Summary field is always NORMAL (ERROR does not exist)
FIL, AT1, AT2, ATS; Stand value 000 (apply to all) is not implemented
SHT command forces all to max attn, filters off, FEE pwr off (pre-INI state)
Power supply interface not implemented (RXP, FEP)
Temperature Sensors are not installed, but RPT mock data

Caveat:  The Din (feedback logic) is not implemented, so there is no guarantee that the SPI commands issued actually changed the setting in the ARX boards

Rabbit RCM4200 SPI Bus pins:
SCLK on PE7
DOUT (MOSI) on PC2
DIN (MISO) on PE3
CS on PB7

$Rev$
$LastChangedBy$
$LastChangedDate$

************************************************/

#define TCPCONFIG           1
#define MY_IP_ADDRESS "10.1.1.40"		// ASP IP Address
#define _PRIMARY_STATIC_IP "10.1.1.40"
#define MY_NETMASK    "255.255.255.0"	// MCS network netmask
#define _PRIMARY_NETMASK "255.255.255.0"
#define MY_GATEWAY    "10.1.1.2"		// Network Gateway
#define MY_NAMESERVER "10.1.1.2"		// Network nameserver

#define UDP_BUF_SIZE 512
#define MAX_UDP_SOCKET_BUFFERS 8

#define SPI_SER_C
#define SPI_RX_PORT SPI_RX_PE
#define SPI_CLK_DIVISOR 100

#use "spi.lib"
#use rcm42xx.lib

#memmap xmem
#use "dcrtcp.lib"

#include aspMIB.h
#include aspCommon.h
#include aspFunctions.h
#include aspMCSInterface.h


// Initialize the socket and IP address for MCS
udp_Socket sockIn, sockOut;
longword ip;

// Initialize the buffers
char buf1[512];
char buf2[512];

// Initialize the MIB structure and command queue
aspMIB mib;
aspCommandQueue queue;

main() {
	int status;
     unsigned long int T1, T2;
     ip = resolve(REMOTE_IP);

     initMIB(&mib);
     initQueue(&queue);
	setSPIconst();
	brdInit();

	WrPortI(PCFR,  &PCFRShadow,  PCFRShadow  | 0x44);		// Serial Port C
	WrPortI(PEAHR, &PEAHRShadow, PEAHRShadow | 0xC0);		// Serial Port C
	WrPortI(PEDDR, &PEDDRShadow, PEDDRShadow | 0x80);		// Serial Port C clock on PE7
	WrPortI(PEFR,  &PEFRShadow,  PEFRShadow  | 0x80);		// Serial Port C
	SPIinit();

	// Start network and wait for interface to come up.
	sock_init_or_exit(1);

	// Open RX port
	if( !udp_open(&sockIn, LOCAL_PORT, ip, 0, NULL) ) {
		printf("udp_open of inbound socket failed!\n");
		exit(0);
	}

     // Open TX port
     if( !udp_open(&sockOut, 0, ip, REMOTE_PORT, NULL) ) {
	    printf("udp_open of outbound socket failed!\n");
	    exit(0);
	}

	// Main program loop
	for(;;) {
     	costate {
          	// Receive & transmit packets
	          tcp_tick(NULL);

	          // Receive UDP packet
               T1 = MS_TIMER;
               if( udp_recv(&sockIn, buf1, 512) != -1 ) {
               	// Interpret the packet as a command
	               status = interpertCommand(&mib, &queue, &buf1);

                    if( status != -1 ) {
                   		// Generate reply
                         T2 = MS_TIMER;
	                    generateResponse(&mib, (T2-T1), status, &buf2);

	                    // Send reply
	                    if( udp_send(&sockOut, buf2, 128) < 0 ) {
	                         printf("Error sending datagram!  Closing and reopening socket...\n");
	                    }

	                    // Receive & transmit packets
	              		tcp_tick(NULL);
                   	}

			// Close and re-open
			udp_close(&sockIn);
			udp_open(&sockIn, LOCAL_PORT, ip, 0, NULL);
	          } else {
	               yield;
	          }
          }

		costate {
			// run command in the queue
			processQueue(&mib, &queue);
               waitfor( DelayMs(250) );
		}

          /*
          costate {
          	// update power supply info
               updatePS(mib);
               waitfor( DelaySec(60) );
          }
          */
	}
}
