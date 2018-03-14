import os
import signal
import subprocess
import time

SPAWN_PROCESS_COMMAND = './infinite_loop'

def process_count():
    return len(os.listdir('/proc'))
    
for i in range(5):    
    print('Initial process count: ', process_count())
    print('Spawning new process (C infinite loop)...')
    new_process = subprocess.Popen(SPAWN_PROCESS_COMMAND,
             stdout=subprocess.PIPE, shell=True, preexec_fn=os.setsid)
             # Not sure which of these extra 3 parameters are necessary, but
             # just subprocess.Popen(SPAWN_PROCESS_COMMAND) doesn't work
    time.sleep(1) # parameter is in seconds
    print('Current process count: ', process_count())
    time.sleep(2)
    print('Killing spawned process...')
    os.killpg(os.getpgid(new_process.pid), signal.SIGTERM)
    time.sleep(1)
    print('Final process count: ', process_count())
    time.sleep(2)
