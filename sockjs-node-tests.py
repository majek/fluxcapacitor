#!/usr/bin/env python

import os
import time
import signal

server_pid = os.fork()
if server_pid == 0:
    # child

    os.chdir(os.path.expanduser("~/sockjs-node"))
    os.execv("/home/marek/.node-v0.8.10/bin/node", ["node", "examples/test_server2/server.js"])
    os._exit(0)
    
# parent
time.sleep(1)
os.chdir(os.path.expanduser("~/sockjs-protocol"))
os.system("./venv/bin/python sockjs-protocol-0.3.3.py")
os.kill(server_pid, signal.SIGINT)
