#!/bin/bash

ls /lwa/runtime/runtime*.gz | xargs -n1 ~ops/uploadLogfile.py
ls /data/*-power*gz | xargs -n1 ~ops/uploadLogfile.py
ls /data/temp*gz | xargs -n1 ~ops/uploadLogfile.py
