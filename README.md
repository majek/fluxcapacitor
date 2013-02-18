Fluxcapacitor
----

Fluxcapacitor is a tool for Linux created to speed up tests by lying
about time.

It is somewhat similar to things like:

 * [FreezeGun](http://stevepulec.com/freezegun/) for Python
 * [TimeCop](https://github.com/travisjeffery/timecop) or
   [DeLorean](https://github.com/bebanjo/delorean) for Ruby

While these tools patch time libraries in Ruby and Python,
`fluxcapacitor` works on a lower layer and "patches" low-level
syscalls. That way it can lie about time to any program in any
programming language, as long as it runs on Linux.

This approach has a significant advantage: it is possible to lie about
time to more than a one processes at the same time. It is especially
useful for testing network applications where server and client run in
different processess and somewhat rely on time.

Internally Fluxcapacitor uses on `ptrace` syscall and `LD_PRELOAD`
linker feature and thus is Linux-specific.


Basic examples
----

When you run `sleep` bash command, well, it will block the console for
a given time. For example:

    $ sleep 12

will halt terminal for 12 seconds. When you run it with
`fluxcapacitor`:

    $ ./fluxcapacitor --libpath=. -- sleep 12

it will finish instantly. Cool, ha? To illustrate this:

    $ time sleep 12
    real    0m12.003s

while:

    $ time ./fluxcapacitor --libpath=. -- sleep 12
    real    0m0.057s

Another example, take a look at this session:

    $ date
    Thu Feb 14 23:49:55 GMT 2013
    $ ./fluxcapacitor --libpath=. -- bash -c "date; sleep 120; date"
    Thu Feb 14 23:49:57 GMT 2013
    Thu Feb 14 23:51:57 GMT 2013
    $ date
    Thu Feb 14 23:49:58 GMT 2013


You should see a program thinks time had passed, although it did not
in reality.

`Fluxcapacitor` works with any programming language:

    $ ./fluxcapacitor --libpath=. -- python -c "import time; time.sleep(1000)"


How does it work
----

Fluxcapacitor internally does two things:

1) It forces `fluxcapacitor_preload.so` to be preloaded using the
`LD_PRELOAD` linux facility. This library is responsible for two things:

   - It makes sure that `clock_gettime()` will use the standard
     syscall, not the ultra-fast VDSO mechanism. That gives us the
     opportunity to replace the return value of the system call later.
   - It replaces various time-related libc functions: `clock_gettime()`,
     `gettimeofday()`, `time()`, `ftime()`, `nanosleep()` and `clock_nanosleep()`
     with variants using modified `clock_gettime()`. That simplifies syscall
     semantics thus making some parts of the server code less involved.

2) It runs given command, and its forked children, in a `ptrace()`
sandbox capturing all the syscalls. Some syscalls - notably
`clock_gettime` have original results returned by kernel overwritten
by faked values. Other syscalls, like `select()`, `poll()` and
`epoll_wait()` can be interrupted (by a signal) and the result will be
set to look like a timeout had expired.


When it won't work
----

Fluxcapacitor won't work in a number of cases:

1) If your code is statically compiled and `fluxcapacitor_preload.so`
ld-preloaded library can't play its role.

2) If your code uses unpopular functions in the event loop, like:
`signalfd()`, `sigwait()`, `wait()`, `waitpid()`, etc. Or if your
program relies heavily on signals and things like `alert()`,
`setitimer()` or `timerfd_create()`.

3) If your code uses file access or modification
timestamps. `Fluxcapacitor` does not mock that.

Basically, for Fluxcapacitor to work all the time queries need to be
done using `gettimeofday()` or `clock_gettime()` and all the waiting
for timeouts must rely on `select()`, `poll()` or
`epoll_wait()`. Fortunately that's the case in most programming
languages.


Advanced usage
----

Say you have a delayed-echo server and you want to test it. It echos
messages, just delayed by a few seconds. You don't want your tests to
take too long. For example the code:

 - [server.py](https://github.com/majek/fluxcapacitor/blob/master/examples/slowecho/server.py)
 - [tests.py](https://github.com/majek/fluxcapacitor/blob/master/examples/slowecho/tests.py)

Normally you could run the server, run the tests in a separate console
and wait for some time. With `fluxcapacitor` you write a
[wrapper program](https://github.com/majek/fluxcapacitor/blob/master/examples/slowecho/run_test.py):


```python
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
```

This script just runs the tests in an automated manner. Normally the
tests take 1 second each:

    $ time python run_test.py
    real    0m5.112s

With `fluxcapacitor` it's much faster:

    $ ./fluxcapacitor --libpath=. -- python run_test.py
    real    0m0.355s


Development
====

Building
----

To compile `fluxcapacitor` you need a reasonably recent linux
distribution. Type:

    make build

Testing
----

`Fluxcapacitor` comes with a number of python tests. See `tests`
subdirectory for details. To test `fluxcapacitor` type:

    make test

or just:

    make

You can also run specific tests, but that's a bit more complex. For
example to run `SingleProcess.test_sleep` from `tests/tests_basic.py`:

    FCPATH="$PWD/fluxcapacitor --libpath=$PWD" \
        python tests/tests_basic.py SingleProcess.test_sleep

