import tests
from tests import at_most, compile
import subprocess

class SingleProcess(tests.TestCase):
    @at_most(seconds=0.5)
    def test_sleep(self):
        self.system("sleep 1")

    @at_most(seconds=0.5)
    def test_python_select(self):
        self.system('python -c "import select; select.select([],[],[], 1)"')

    @at_most(seconds=0.5)
    def test_python_poll(self):
        self.system('python -c "import select; select.poll().poll(1000)"')

    @at_most(seconds=0.5)
    def test_python_epoll(self):
        self.system('python -c "import select; select.epoll().poll(1000)"')

    @at_most(seconds=0.5)
    def test_node_epoll(self):
        self.system('node -e "setTimeout(function(){},1000);"')


    @at_most(seconds=0.5)
    @compile(code='''
#include <unistd.h>

int main() {
    sleep(1);
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
