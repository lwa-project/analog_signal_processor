#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Functions to run processes, particularly those that run on the individual DP boards.
"""

import datetime
import math
import os
import re
import string
import struct
import subprocess
import sys
import thread
import time
import select
import shutil

import logging

from signal import SIGKILL

__version__ = '0.1'
__revision__ = '$Rev$'
__all__ = ['flushfile', 'run_process', 'run_processes', 'spawn_process', 'spawn_processes', '__version__', '__revision__', '__all__']


runLogger = logging.getLogger('__main__')


class flushfile(object):
	"""
	Object that wraps an open file-like object in such a way that all calls to 
	write() are imediately flush()'d without any additional effort.
	"""
	
	def __init__(self, f):
		self.f = f
		
	def write(self, x):
		self.f.write(x)
		self.f.flush()

def run_process(name, command, logfilename, timeout=-1):
	"""
	Run a process in the background with pipes.
	"""

	ztime = '%04d-%02d-%02d %02d:%02d:%02d' % time.gmtime()[:6]
	runLogger.debug("run_process: at %s running %s", ztime, str(command))

	logfile = logfilename
	log = open(logfile, "a+")

	#
	# create pipes for log and error messages
	#
	(out_r, out_w) = os.pipe()
	(err_r, err_w) = os.pipe()
	
	#
	# initialize output file handles and start times
	#

	pid = os.fork() 
	if pid > 0:
		# this is the child process
		#
		# close unused file descriptors
		#
		os.close(out_w)
		os.close(err_w)
		ztime = '%04d-%02d-%02d %02d:%02d:%02d' % time.gmtime()[:6]
		text = '%s Starting %s pid %d (%s)\n' % (ztime, name, pid, command[0])
		log.write(text)
		flushfile(log)

		#
		# loop as long as pipes are open (or until a timeout is reached)
		#
		t0 = time.time()

		opened = [out_r, err_r] 
		while opened != []:
			ready = select.select(opened, [], [])
			for fd in ready[0]:
				ztime = '%04d-%02d-%02d %02d:%02d:%02d' % time.gmtime()[:6]
				text = os.read(fd, 1024)
				if text == '':
					#
					# close pipe
					#
					os.close(fd)
					opened.remove(fd)
				elif fd == out_r:
					log.write(ztime + ' ' + text)
					flushfile(log)
				else:
					log.write(ztime + ' ' + text)
					flushfile(log)
				
				if timeout > 0:
					t1 = time.time()
					if t1-t0 > timeout:
						os.kill(pid, SIGKILL)
						(pid, status) = os.waitpid(pid, 0)
						ztime = '%04d-%02d-%02d %02d:%02d:%02d' % time.gmtime()[:6]
						text = '%s %s Process %d (%s) exited with status %d (timeout)\n' % (ztime, name, pid, command[0], status)
						log.write(text)
						flushfile(log)
						
						raise OSError('Timeout reached on PID %i (%s)' % (pid, ' '.join(command)))
		#
		# get process' exit status, and log termination message
		#
		(pid, status) = os.waitpid(pid, 0)
		ztime = '%04d-%02d-%02d %02d:%02d:%02d' % time.gmtime()[:6]
		text = '%s %s Process %d (%s) exited with status %d\n' % (ztime, name, pid, command[0], status)
		log.write(text)

	else:
		# this is the parent process
		#
		# close stdin and connect stdout and stderr to pipes
		#
		os.close(0)
		a = os.open('/dev/null', os.O_RDONLY)
		os.close(1)
		b = os.dup(out_w)
		os.close(2)
		c = os.dup(err_w)

		#
		# close unused file descriptors
		#
		for fd in (out_r, out_w, err_r, err_w):
			os.close(fd)

		#
		# start up the daemon process
		#
		os.execvp(command[0], command)
		os._exit(1)


def run_processes(name, command, board_dict, logfilename, timeout=-1):
	"""
	Run a process on each board in the board dictionary.
	"""
    
	for slot in board_dict:
		hostname = board_dict[slot]
		# if the slot is non-empty
		if ((hostname != "None") and (hostname != "No response")):
			# send command to hostname
			new_command = ["/usr/local/bin/"+hostname]
			for entry in command:
				new_command.append(entry)
			run_process(name, new_command, logfilename, timeout=timeout)


def spawn_process(name, command, logfilename):
	"""
	Spawn a process in the background without blocking.
	"""

	ztime = '%04d-%02d-%02d %02d:%02d:%02d' % time.gmtime()[:6]
	runLogger.debug("spawn_process: at %s running %s", ztime, str(command))

	logfile = logfilename
	log = open(logfile, "a+")
	
	process = subprocess.Popen(command, stdout=log, stderr=log)    
	ztime = '%04d-%02d-%02d %02d:%02d:%02d' % time.gmtime()[:6]
	text = '%s Starting %s pid %d (%s)\n' % (ztime, name, process.pid, command[0])
	log.write(text)
	flushfile(log)    
	return process

	# We don't log when the PID finishes here. If a way to do this without blocking
	# is found, that would be a good augmentation here
	# But, speed and not blocking is of the essance here!


def spawn_processes(name, command, board_dict, logfilename):
	"""
	Spawn a process on each board in the board dictionary without blocking.
	"""
	
	for slot in board_dict:
		hostname = board_dict[slot]
		# if the slot is non-empty
		if ((hostname != "None") and (hostname != "No response")):
			# send command to hostname
			new_command = ["/usr/local/bin/"+hostname]
			for entry in command:
				new_command.append(entry)
			spawn_process(name, new_command, logfilename)



if __name__ == '__main__':
	argc = len(sys.argv)   
	sys.exit()
