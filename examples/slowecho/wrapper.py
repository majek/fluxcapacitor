#!/usr/bin/env python

import os
import time
import signal

server_pid = os.fork()
if server_pid == 0:
    os.execv("/usr/bin/python", ["python", "server.py"])
    os._exit(0)
else:
    time.sleep(1)
    os.system("python tests.py")
    os.kill(server_pid, signal.SIGINT)
