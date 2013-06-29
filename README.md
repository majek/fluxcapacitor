Fluxcapacitor
----

`Fluxcapacitor` is a tool for making your program run without blocking on timeouts, on functions like `poll` and `select`, by spoofing POSIX time functions.

It is somewhat similar to:

 * [FreezeGun](http://stevepulec.com/freezegun/) for Python
 * [TimeCop](https://github.com/travisjeffery/timecop) or
   [DeLorean](https://github.com/bebanjo/delorean) for Ruby

While these tools patch time libraries in Ruby and Python,
`fluxcapacitor` works on a lower layer by "patching" low-level
syscalls. That way, it can lie about time to any program in any
programming language, as long as it runs on Linux.

This approach has a significant advantage: it is possible to lie about
time to many processes at the same time. It is especially useful for
running network applications where server and client run in different
processes which rely on time. It will also work with multithreaded 
applications.

Another comparable project is:

 * [libfaketime](https://github.com/wolfcw/libfaketime)

`Fluxcapacitor` is fundamentally different from `libfaketime`, which
can fake the time functions, but doesn't affect the runtime of the
program. Conversely, `fluxcapacitor` will make your program run faster
and be 100% CPU constrained. It does that by "speeding up" blocking
syscalls. Faking time is a neccessary side effect.


Internally,`Fluxcapacitor` uses `ptrace` on syscalls and `LD_PRELOAD`,
which is why it's Linux specific.

<b>Join the <a href="https://groups.google.com/group/fluxcapacitor-dev/subscribe"><code>fluxcapacitor-dev</code> mailing list</b></a>. Or view <a href="http://groups.google.com/group/fluxcapacitor-dev">the archives</a>.


Basic examples
----

When you run `sleep` bash command, well, it will block the console for
a given time. For example:

    $ sleep 12

will halt terminal for 12 seconds. When you run it with
`fluxcapacitor`:

    $ ./fluxcapacitor -- sleep 12

it will finish instantly. Cool, huh? To illustrate this:

    $ time sleep 12
    real    0m12.003s

while:

    $ time ./fluxcapacitor -- sleep 12
    real    0m0.057s

Another example, take a look at this session:

    $ date
    Thu Feb 14 23:49:55 GMT 2013
    $ ./fluxcapacitor -- bash -c "date; sleep 120; date"
    Thu Feb 14 23:49:57 GMT 2013
    Thu Feb 14 23:51:57 GMT 2013
    $ date
    Thu Feb 14 23:49:58 GMT 2013


You should see a program thinks time had passed, although it did not
in reality.

Ever heard of the
[year 2038 problem](https://en.wikipedia.org/wiki/Year_2038_problem)?
Here's how it's going to look like in action:

    $ ./fluxcapacitor -- bash -c "sleep 700000000; date"
    Thu Apr 26 17:44:25 BST 2035
    $ ./fluxcapacitor -- bash -c "sleep 800000000; date"
    Wed May 21 20:04:03 GMT 1902


Finally, `fluxcapacitor` works with any programming language:

    $ ./fluxcapacitor -- python -c "import time; time.sleep(1000)"


For reference, `fluxcapacitor` usage info:

```
$ ./fluxcapacitor --help
Usage:

    fluxcapacitor [options] [ -- command [ arguments ... ] ... ]

Options:

  --libpath=PATH       Load fluxcapacitor_preload.so from
                       selected PATH directory.
  --signal=SIGNAL      Use specified signal to interrupt blocking
                       syscall instead of SIGURG.
  --idleness=TIMEOUT   Speed up time only when all processess were
                       idle for more than TIMEOUT (50ms by default).
  --verbose,-v         Print more stuff.
  --help               Print this message.
```


How does it work
----

Fluxcapacitor internally does two things:

1) It forces `fluxcapacitor_preload.so` to be preloaded using the
`LD_PRELOAD` linux facility. This library is responsible for two
things:

   * It makes sure that `clock_gettime()` will use the standard
     syscall, not the ultra-fast VDSO mechanism. That gives us the
     opportunity to replace the return value of the system call later.
   * It replaces various time-related libc functions:
     `clock_gettime()`, `gettimeofday()`, `time()`, `ftime()`,
     `nanosleep()` and `clock_nanosleep()` with variants using
     modified `clock_gettime()`. That simplifies syscall semantics
     thus making some parts of the server code less involved.

2) It runs then given command and its forked children in a 
`ptrace()` sandbox, capturing all syscalls. Some syscalls - notably 
`clock_gettime`, have their original results returned from the kernel 
overwritten by faked values. Other syscalls, like `select()`, 
`poll()` and`epoll_wait()`, can be interrupted (by a signal) and the 
result will be set to look like a timeout has expired. Full list of 
recognized syscalls that can be sped up:

   * `epoll_wait()`, `epoll_pwait()`
   * `select()`, `_newselect()`, `pselect6()`
   * `poll()`, `ppoll()`
   * `nanosleep()`

### Speeding up

Fluxcapacitor monitors all syscalls run by the child processes. 
All syscalls are relayed to the kernel, as normal. This operation 
continues until fluxcapacitor notices that all the child processes 
are waiting on recognised time-related syscalls, like `poll` or 
`select`. When that happens, fluxcapacitor decides to speed up the time.
It advances the internal timer and sends a signal (SIGURG by default) 
to the process that is blocked with the smallest timeout value. 
Fluxcapacitor is then woken up by the kernel to give it a chance to 
pass the signal to the child. It swallows the signal and sets the return 
value of the syscall to look like a timeout had expired. See diagram:

```
  child          fluxcapacitor              kernel
  -----          -------------              ------

   |
   +--- select(1s) -->+
                      |
                      +------------------------>

                      kill(child, SIGURG)

                      +<---- signal received ---
                      |
                      (pretend it was a timeout)
                      |
   +<--- timeout -----+
   |
```


When it won't work
----

Fluxcapacitor won't work in a number of cases:

1) If your code is statically compiled and `fluxcapacitor_preload.so`
   ld-preloaded library can't play its role.

2) If your code uses unpopular blocking functions in the event loop,
   like `signalfd()` and `sigwait()`, or if your program relies 
   heavily on signals and things like `alert()`, `setitimer()`, or
   `timerfd_create()`.

3) If your code uses file access or modification
   timestamps. `Fluxcapacitor` does not mock that.

Basically, for Fluxcapacitor to work all the time, queries need to be
done using `gettimeofday()` or `clock_gettime()`, and all the waiting
for timeouts must rely on `select()`, `poll()` or
`epoll_wait()`. Fortunately, that's the case in most programming
languages.


Advanced usage
----

`Fluxcapacitor`'s main application is speeding up tests.

Say you have a "delayed echo" server and you want to test it. It echos
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

    $ ./fluxcapacitor -- python run_test.py
    real    0m0.355s


Development
====

Prerequisites
----

To compile the things you need are `git`, `gcc` and `make`. This 
should do:

    $ sudo yum git gcc make

or

    $ sudo apt-get install git gcc make

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
example to run `SingleProcess.test_bash_sleep` from `tests/tests_basic.py`:

    FCPATH="$PWD/fluxcapacitor --libpath=$PWD" \
        python tests/tests_basic.py SingleProcess.test_bash_sleep

