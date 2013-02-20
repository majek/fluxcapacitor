import os
import tests
from tests import at_most, compile
import subprocess

class SingleProcess(tests.TestCase):
    @at_most(seconds=0.5)
    def test_bash_sleep(self):
        self.system("sleep 10")

    @at_most(seconds=0.5)
    def test_bash_bash_sleep(self):
        self.system("bash -c 'sleep 120;'")

    @at_most(seconds=0.5)
    def test_python_select(self):
        self.system('python -c "import select; select.select([],[],[], 10)"')

    @at_most(seconds=0.5)
    def test_python_poll(self):
        self.system('python -c "import select; select.poll().poll(10000)"')

    @at_most(seconds=0.5)
    def test_python_epoll(self):
        self.system('python -c "import select; select.epoll().poll(10000)"')

    @at_most(seconds=0.5)
    def test_node_epoll(self):
        if os.system("node -v >/dev/null 2>/dev/null") != 0:
            print "ignoring nodejs test"
        else:
            self.system('node -e "setTimeout(function(){},10000);"')

    def test_bad_command(self):
        self.system('command_that_doesnt exist',
                    returncode=127, ignore_stderr=True)

    def test_return_status(self):
        self.system('python -c "import sys; sys.exit(188)"', returncode=188)
        self.system('python -c "import sys; sys.exit(-1)"', returncode=255)


    @at_most(seconds=0.5)
    @compile(code='''
#include <unistd.h>
int main() {
    sleep(10);
    return(0);
}
''')
    def test_sleep(self, compiled=None):
        self.system(compiled)


    @at_most(seconds=0.5)
    @compile(code='''
#include <time.h>
int main() {
    struct timespec ts = {1, 0};
    nanosleep(&ts, NULL);
    return(0);
}
''')
    def test_nanosleep(self, compiled=None):
        self.system(compiled)




if __name__ == '__main__':
    import unittest
    unittest.main()
