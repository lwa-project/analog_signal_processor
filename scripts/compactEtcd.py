#!/usr/bin/env python3

import sys
import etcd3
import yaml
import argparse


def main(args):
    # Load in the etcd server configuration
    with open(args.config, 'r') as fh:
        etcd_config = yaml.load(fh, yaml.loader.Loader)
    host, port = etcd_config['endpoints'][0].split(':')
    port = int(port, 10)
    if args.verbose:
        print(f"Connecting to {host}, port {port}")
        
    # Connect
    client = etcd3.Etcd3Client(host, port)
    
    # Find the earliest active revision in the database
    for key in client.get_all():
        try:
            if key[1].mod_revision < min_revision:
                min_revision = key[1].mod_revision
                min_rev_key = key[1].key
        except NameError:
            min_revision = key[1].mod_revision
            min_rev_key = key[1].key    
    if args.verbose:
        print(f"Earliest active revision is {min_revision} for key '{min_rev_key.decode()}'")
        
    # Prune so that we keep at most the previous 10,000 revisions
    if min_revision == 0:
        min_revision = 1
    min_revision = ((min_revision-1)//args.keep)*args.keep
    if args.verbose:
        print(f"Compacting up to {min_revision}")
        
    # Compact, defragment, and clear any alarm conditions
    try:
        client.compact(min_revision, physical=True)
        client.defragment()
        client.disarm_alarm()
    except Exception as e:
        if args.verbose:
            print(f"Warning: {str(e)}")
            
    # Report on database size
    if args.verbose:
        status = client.status()
        print(f"Database size is {status.db_size} B")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
            description='compact the ASP-MCS etcd database to keep its size in check', 
            formatter_class=argparse.ArgumentDefaultsHelpFormatter
            )
    parser.add_argument('-c', '--config', type=str, default='/lwa/software/backend/etcdConfig.yml',
                        help='backend etcd YAML configuration file')
    parser.add_argument('-k', '--keep', type=int, default=10000,
                        help='maximum number of revisions to keep')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='be verbose about what is happening')
    args = parser.parse_args()
    main(args)
