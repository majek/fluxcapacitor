import functools
import unittest
import time
import os
import subprocess
import tempfile

debug = False

class TestCase(unittest.TestCase):
    def setUp(self):
        self.fcpath = os.getenv('FCPATH', None)
        assert self.fcpath is not None, "Set FCPATH environment variable first!"

    def tearDown(self):
        pass

    def do_system(self, cmd, returncode=0):
        rc = subprocess.call(cmd, shell=True)
        self.assertEqual(rc, returncode)
        return True

    def system(self, cmd, returncode=0):
        if self.fcpath:
            final_cmd = "%s -- %s" % (self.fcpath, cmd)
        else:
            final_cmd = cmd
        if debug:
            print "debug: %r" % (final_cmd,)
        rc = subprocess.call(final_cmd, shell=True)
        self.assertEqual(rc, returncode)
        return True


def at_most(seconds=None):
    def decorator(fn):
        @functools.wraps(fn)
        def wrapper(self, *args, **kwargs):
            t0 = time.time()
            ret = fn(self, *args, **kwargs)
            t1 = time.time()
            td = t1 - t0
            self.assertTrue(td <= seconds,
                            "Task took %.1f, not %.1f seconds" \
                                % (td, seconds))
            return ret
        return wrapper
    return decorator


def compile(code=None):
    def decorator(fn):
        @functools.wraps(fn)
        def wrapper(self, *args, **kwargs):
            (fd, compiled) = tempfile.mkstemp()
            os.close(fd)
            (fd, source) = tempfile.mkstemp(suffix=".c")
            os.write(fd, code + '\n')
            os.close(fd)
            try:
                cc_cmd = "%s %s -Os -Wall %s -o %s" \
                    % (os.getenv('CC', 'cc'), os.getenv('CFLAGS', ''),
                       source, compiled)
                rc = subprocess.call(cc_cmd, shell=True)
                self.assertEqual(rc, 0)

                kwargs['compiled'] = compiled
                ret = fn(self, *args, **kwargs)
            finally:
                if not debug:
                    os.unlink(source)
                    os.unlink(compiled)
                else:
                    print "debug: not unlinked %s %s" % (source, compiled)
            return ret
        return wrapper
    return decorator
