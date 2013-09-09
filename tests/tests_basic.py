import os
import tests
from tests import at_most, compile, savefile
import subprocess


node_present = True
erlang_present = True

if os.system("node -v >/dev/null 2>/dev/null") != 0:
    print " [!] ignoring nodejs tests"
    node_present = False

if (os.system("erl -version >/dev/null 2>/dev/null") != 0 or
    os.system("which escript >/dev/null 2>/dev/null") != 0):
    print " [!] ignoring erlang tests"
    erlang_present = False


sleep_sort_script='''\
#!/bin/bash
echo "Unsorted: $*"
function f() {
    sleep "$1"
    echo -n "$1 "
}
while [ -n "$1" ]; do
    f "$1" &
    shift
done
wait
echo
'''

class SingleProcess(tests.TestCase):
    @at_most(seconds=2)
    def test_bash_sleep(self):
        self.system("sleep 10")

    @at_most(seconds=2)
    def test_bash_bash_sleep(self):
        self.system("bash -c 'sleep 120;'")


    @at_most(seconds=2)
    def test_python_sleep(self):
        self.system('python -c "import time; time.sleep(10)"')

    @at_most(seconds=2)
    def test_python_select(self):
        self.system('python -c "import select; select.select([],[],[], 10)"')

    @at_most(seconds=2)
    def test_python_poll(self):
        self.system('python -c "import select; select.poll().poll(10000)"')

    @at_most(seconds=2)
    def test_python_epoll(self):
        self.system('python -c "import select; select.epoll().poll(10000)"')


    @at_most(seconds=2)
    def test_node_epoll(self):
        if node_present:
            self.system('node -e "setTimeout(function(){},10000);"')


    def test_bad_command(self):
        self.system('command_that_doesnt exist',
                    returncode=127, ignore_stderr=True)

    def test_return_status(self):
        self.system('python -c "import sys; sys.exit(188)"', returncode=188)
        self.system('python -c "import sys; sys.exit(-1)"', returncode=255)


    @at_most(seconds=2)
    @compile(code='''
    #include <unistd.h>
    int main() {
        sleep(10);
        return(0);
    }''')
    def test_c_sleep(self, compiled=None):
        self.system(compiled)


    @at_most(seconds=2)
    @compile(code='''
    #include <time.h>
    int main() {
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
        return(0);
    }''')
    def test_c_nanosleep(self, compiled=None):
        self.system(compiled)



    @at_most(seconds=5)
    @savefile(suffix="erl", text='''\
    #!/usr/bin/env escript
    %%! -smp disable +A1 +K true -noinput
    -export([main/1]).
    main(_) ->
        timer:sleep(10*1000),
        halt(0).
    ''')
    def test_erlang_sleep(self, filename=None):
        if erlang_present:
            self.system("escript %s" % (filename,))

    @at_most(seconds=5)
    @savefile(suffix="erl", text='''\
    #!/usr/bin/env escript
    %%! -smp enable +A30 +K true -noinput
    -export([main/1]).
    main(_) ->
        timer:sleep(10*1000),
        halt(0).
    ''')
    def test_erlang_sleep_smp(self, filename=None):
        if erlang_present:
            self.system("escript %s" % (filename,))

    @at_most(seconds=5)
    @savefile(suffix="erl", text='''\
    #!/usr/bin/env escript
    %%! -smp enable +A30 +K false -noinput
    -export([main/1]).
    main(_) ->
        timer:sleep(10*1000),
        halt(0).
    ''')
    def test_erlang_sleep_smp_no_epoll(self, filename=None):
        if erlang_present:
            self.system("escript %s" % (filename,))


    @at_most(seconds=5)
    @savefile(suffix="erl", text='''\
    #!/usr/bin/env escript
    %%! -smp disable +A1 +K true -noinput
    -export([main/1]).
    main(_) ->
        self() ! msg,
        proc(10),
        receive
            _ -> ok
        end.

    proc(0) ->
        receive
            _ -> halt(0)
        end;
    proc(N) ->
        Pid = spawn(fun () -> proc(N-1) end),
        receive
            _ -> timer:sleep(1000),
                 Pid ! msg
        end.
    ''')
    def test_erlang_process_staircase(self, filename=None):
        if erlang_present:
            self.system("escript %s" % (filename,))


    @at_most(seconds=2)
    def test_perl_sleep(self):
        self.system("perl -e 'sleep 10'")


    @at_most(seconds=5)
    @savefile(suffix="sh", text=sleep_sort_script)
    def test_sleep_sort(self, filename=None):
        self.system("bash %s 1 12 1231 123213 13212 > /dev/null" % (filename,))

    @at_most(seconds=5)
    @savefile(suffix="sh", text=sleep_sort_script)
    def test_sleep_sort(self, filename=None):
        self.system("bash %s 5 3 6 3 6 3 1 4 7 > /dev/null" % (filename,))

    @at_most(seconds=10)
    def test_parallel_sleeps(self):
        for i in range(10):
            stdout = self.system(' -- '.join(['bash -c "date +%s"',
                                              'bash -c "sleep 60; date +%s"',
                                              'bash -c "sleep 120; date +%s"']),
                                 capture_stdout=True)
            a, b, c = [int(l) for l in stdout.split()]
            assert 55 < (b - a) < 65, str(b-a)
            assert 55 < (c - b) < 65, str(c-b)
            assert 110 < (c - a) < 130, str(c-a)

    @at_most(seconds=3)
    def test_file_descriptor_leak(self):
        out = subprocess.check_output("ls /proc/self/fd", shell=True)
        normal_fds = len(out.split('\n'))
        stdout = self.system(' -- '.join(['sleep 1',
                                          'sleep 60',
                                          'sleep 120',
                                          'bash -c "sleep 180; ls /proc/self/fd"']),
                             capture_stdout=True)
        after_fork_fds = len(stdout.split('\n'))
        assert normal_fds == after_fork_fds


if __name__ == '__main__':
    import unittest
    unittest.main()
