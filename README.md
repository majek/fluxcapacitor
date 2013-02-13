Fluxcapacitor
----

Fluxcapacitor is a tool for Linux, created to speed up tests.

It is somewhat similar to tools like:

 * [FreezeGun](http://stevepulec.com/freezegun/) for Python
 * [timecop](https://github.com/travisjeffery/timecop) or
   [Delorean](https://github.com/bebanjo/delorean) for Ruby

These tools patch/mock time libraries in a programming
language.

Fluxcapacitor on the other hand "patches" low-level syscalls. That way
it can lie about time to any program in any programming language, as
long as it runs on Linux.

This approach a significant advantage: it is possible to lie about
time to more than a one processes at the same time. It is especially
useful for testing network applications where server and client run in
different processess (and somewhat rely on time).

Internally Fluxcapacitor relies on `ptrace` syscall and `LD_PRELOAD`
linker feature, and thus is very Linux-specific.


Basic examples
----

First, compile:

    make

For example, these commands should finish instantly:

    ./fluxcapacitor --libpath=. -- sleep 1
    ./fluxcapacitor --libpath=. -- python -c "import time; time.sleep(1000)"


How does it work
----

Fluxcapacitor does two things:

1) It forces `fluxcapacitor_preload.so` to be preloaded for the
executed command. This library is responsible for two things:
   - It makes sure that `clock_gettime` will not be used by fast VDSO
     mechanism, but by the standard system call. (that gives us the
     opportunity to replace the return value of the system call
     later).
   - It replaces various time-related libc functions, like `time()`
     and `gettimeofday()` with replacements using `clock_gettime`
     underneath.

2) It runs the command (and its forked children) in a `ptrace()`
sandbox, capturing all the syscalls. Some syscalls - notably
`clock_gettime` will have original results overwritten by faked
values. Other syscalls, like `select()`, `poll()` and `epoll_wait()`
can be interrupted and its return value (`-EINTR`) will be rewritten
to suggest reaching a timeout.


When it won't work
----

Fluxcapacitor won't work in a number of cases:

1) If your code is statically compiled and `fluxcapacitor_preload.so`
can't play its role.

2) If your code uses non file descriptors event loops, like:
`signalfd()`, `sigwait()`, `wait()`, etc. Or if your program relies
heavily on signals and things like `alert()`, `setitimer()` or
`timerfd_create()`.

Basically, for Fluxcapacitor to work all the time queries need to be
done using `gettimeofday()` or `clock_gettime()` and all the waiting
for timeouts must rely on `select()`, `poll()` or `epoll_wait()`.


