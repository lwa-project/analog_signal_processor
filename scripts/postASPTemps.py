#!/usr/bin/env python3

import time
import requests
import subprocess
from socket import gethostname


URL = "https://lwalab.phys.unm.edu/OpScreen/update.php"
KEY = "c0843461abe746a4608dd9c897f9b261"
SITE = gethostname().split("-",1)[0]
SUBSYSTEM = "ASP"

# Get the last line of the log file
test = subprocess.check_output(["tail", "-n1", '/data/temp.txt'], stderr=subprocess.DEVNULL)
test = test.decode('ascii')
test = test.replace('\n', '')

# Check to see if the log is actually getting updated.  If not, send NaNs
try:
    lastUpdated, junk = test.split(',', 1)
except ValueError:
    lastUpdated = 0
    junk = []
lastUpdated = float(lastUpdated)
if time.time() > lastUpdated + 300:
    test = "%.2f,%s" % (time.time(), ','.join(["NaN" for i in range(4)]))

# Send the update to lwalab
f = requests.post(URL,
                  data={'key': KEY, 'site': SITE, 'subsystem': SUBSYSTEM, 'data': test})
f.close()
