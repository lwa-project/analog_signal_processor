#!/bin/bash

ls /lwa/runtime/runtime*.gz | xargs -n1 ~ops/uploadLogfile.py
ls /data/psu*.gz | xargs -n1 ~ops/uploadLogfile.py
ls /data/temp*gz | xargs -n1 ~ops/uploadLogfile.py
ls /home/ops/board*.gz | xargs -n1 ~ops/uploadLogfile.py

