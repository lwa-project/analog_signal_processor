#!/usr/bin/env python3

import os
import sys
import numpy as np
import argparse

from lsl.common.stations import parse_ssmif


def main(args):
    # Parse the station and build up the analog signal mapper
    station = parse_ssmif(args.ssmif)
    ansig = [None,]*len(station.antennas)
    for ant in station.antennas:
        ansig[ant.arx.asp_channel-1] = ant
        
    # Load in the current data
    data = np.loadtxt(args.currents, delimiter=',')
    data = data[:,1:]   # Drop the poll time
    if station.name.lower() == 'lwana':
        ## Special catch for LWA-NA and the Rev H boards that have:
        ## * an extra leading column that contains the total FEE current draw and
        ## * currents are reported in A instead of mA
        data = data[:,1:]
        data *= 1000
    print(f"Loaded current data for {data.shape[1]} antennas spanning {data.shape[0]} polls")
    
    # Run the statistics across time
    mean_mA = data.mean(axis=0)
    std_mA = data.std(axis=0)
    
    # Find the baddies and snitch on them
    bad = np.where((mean_mA < 200) | (std_mA > 100))[0]
    nenufar = np.where((mean_mA > 50) & (mean_mA < 70) & (std_mA < 10))[0]
    if bad.size == 0:
        ## Will this ever be true???
        print(f"No bad front ends with mean < {args.min_mean_current} mA or std > {args.max_std_current} mA")
        
    else:
        print(f"Bad front ends with mean < {args.min_mean_current} mA or std > {args.max_std_current} mA")
        for b in bad:
            ant = ansig[b]
            print(f"  Stand {ant.stand.id:3d}, pol {ant.pol} is bad with mean {mean_mA[b]:5.1f} mA and std {std_mA[b]:5.1f} mA")
            if b in nenufar:
                print("  ^^^ Possible NenuFAR front end ^^^")
                
    if args.plot:
        # Generate the mean/stddev plot to show what's going on
        from matplotlib import pyplot as plt
        
        fig = plt.figure()
        ax = fig.gca()
        ax.scatter(mean_mA, std_mA, marker='o', label='Good')
        if bad.size > 0:
            ax.scatter(mean_mA[bad], std_mA[bad], marker='o', label='Bad')
        l, = ax.plot([50, 70, 70, 50, 50], [-1, -1, 10, 10, -1], color='green')
        if nenufar.size > 0:
            ax.scatter(mean_mA[nenufar], std_mA[nenufar], marker='o', color=l.get_color(), label='NenuFAR?')
        ax.axvline(args.min_mean_current, linestyle='--', color='black')
        ax.axhline(args.max_std_current, linestyle=':', color='black')
        ax.legend(loc=0)
        ax.set_title('Front End Current Analysis')
        ax.set_xlabel('Mean Current [mA]')
        ax.set_ylabel('Current Std. Dev. [mA]')
        plt.show()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="look at ASP's FEE current log to identify potentially bad front ends",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
        )
    parser.add_argument('ssmif', type=str,
                        help='SSMIF to use to make analog signal paths to stands/polarizations')
    parser.add_argument('currents', type=str,
                        help='fee-power.txt from ASP to examine')
    parser.add_argument('-m', '--min-mean-current', type=float, default=200,
                        help='minimum mean current in mA to accept for a good front end')
    parser.add_argument('-s', '--max-std-current', type=float, default=100,
                        help='maximum standard deviation of the current in mA to accept for a good front end')
    parser.add_argument('-p', '--plot', action='store_true',
                        help='show a diganostic plot as well')
    args = parser.parse_args()
    main(args)
