#!/usr/bin/env python2
import socket
import time

HOST = '0.0.0.0'
PORT = 1234
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((HOST, PORT))
s.listen(1)
print 'Listening on %s:%s' % (HOST, PORT)
try:
    while True:
        conn, addr = s.accept()
        print 'Connected by', addr
        while True:
            data = conn.recv(1024)
            if not data:
                break
            time.sleep(2)
            conn.sendall(data)
except KeyboardInterrupt:
    pass

