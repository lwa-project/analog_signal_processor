#!/usr/bin/env python3

import sys
import argparse
import subprocess
sys.path.append('/lwa/software')

import aspSUB20


def main(args):
    # Configure the bus
    for spi_cmd in (aspSUB20.SPI_cfg_shutdown, aspSUB20.SPI_cfg_normal,
                    aspSUB20.SPI_cfg_output_P12_13_14_15, aspSUB20.SPI_cfg_output_P16_17_18_19,
                    aspSUB20.SPI_cfg_output_P20_21_22_23, aspSUB20.SPI_cfg_output_P24_25_26_27,
                    aspSUB20.SPI_cfg_output_P28_29_30_31):
        cmd = ['/usr/local/bin/sendARXDevice', args.device, str(args.nboard*8)]
        for i in range(8*args.nboard):
            cmd.extend([str(i+1), "0x%x" % spi_cmd])
        try:
            subprocess.check_call(cmd)
        except subprocess.CalledProcessesError:
            pass
            
    # Turn on all of the board ID lights
    cmd = ['/usr/local/bin/sendARXDevice', args.device, str(args.nboard*8)]
    for i in range(8*args.nboard):
        cmd.extend([str(i+1), "0x%x" % aspSUB20.SPI_P12_on])
    try:
        subprocess.check_call(cmd)
    except subprocess.CalledProcessesError:
            pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Utility to help find bad board-to-board SPI cables',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
        )
    parser.add_argument('nboard', type=int,
                        help='total number of ARX boards')
    parser.add_argument('-d', '--device', type=str, default='/dev/ttyACM0',
                        help='ARX comm device name or serial number')
    args = parser.parse_args()
    main(args)
