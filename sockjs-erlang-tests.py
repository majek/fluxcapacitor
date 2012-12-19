#!/usr/bin/env python

import os
import time
import signal

server_pid = os.fork()
if server_pid == 0:
    # child

    os.chdir(os.path.expanduser("~/sockjs-erlang"))
    path = "/home/marek/.erlang-R15B02/bin:" + os.getenv("PATH")
    os.putenv("PATH", path)
    args = "./examples/cowboy_test_server.erl".split(" ")
    os.execv(args[0], args)
    os._exit(0)
    
# parent
time.sleep(1)
os.chdir(os.path.expanduser("~/sockjs-protocol"))
os.system("./venv/bin/python sockjs-protocol-0.3.3.py")
os.kill(server_pid, signal.SIGINT)
