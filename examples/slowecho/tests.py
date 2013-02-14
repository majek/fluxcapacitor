#!/usr/bin/env python
'''
>>> s = connect()
>>> s.sendall('abc')
>>> s.recv(128)
'abc'
>>> s.sendall('def')
>>> s.recv(128)
'def'
'''

import socket

def connect(host='127.0.0.1', port=1234):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))
    return s

if __name__ == "__main__":
    import doctest
    doctest.testmod(verbose=True)
