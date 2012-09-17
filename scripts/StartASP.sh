#!/bin/bash

case "$1" in
'start')
  echo -n "Starting ASP MCS..."

  date >> /lwa/runtime/start
  echo "starting asp_cmnd.py" >> /lwa/runtime/runtime.log

  cd /lwa/software
  python /lwa/software/asp_cmnd.py -c /lwa/software/defaults.cfg -d -l /lwa/runtime/runtime.log

  echo "done"
  ;;

'stop')
  echo -n "Stopping ASP MCS..."

  # Find the shl_cmnd.py process
  pid=`pgrep -f asp_cmnd.py`
  if [[ $pid -ne "" ]]; then
    # If it's running, kill it softly and wait to see what happens
    kill $pid
  
    # Give it up to 30 seconds to comply.  After that...  SIGKILL
    while [[ $pid -ne "" ]]; do 
      sleep 5
      ((t += 5))

      pid=`pgrep -f asp_cmnd.py`
      if (( t > 30 )); then 
        kill -s SIGKILL $pid
      fi
    done
  fi

  echo "done"
  ;;

'restart')
  stop
  start
  ;;

'status')
  # Find the asp_cmnd.py process
  pid=`pgrep -f asp_cmnd.py`
  if [[ $pid -ne "" ]]; then
    echo "ASP MCS appears to be running with PID $pid"
  else
    echo "ASP MCS does *not* appear to be running"
  fi
  ;;

esac
