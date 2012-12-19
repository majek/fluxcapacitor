LIBNAME=fluxcapacitor_preload.so
TESTLIBNAME=fluxcapacitor_test.so
LOADERNAME=fluxcapacitor

LDOPTS=-lrt -ldl -rdynamic  
COPTS=-g -Wall -Wextra -Wno-unused-parameter -O3
# -fprofile-arcs -pg

TESTLIB_FILES=testlib.c
LIB_FILES=preload.c
LOADER_FILES=wrapper.c parent.c loader.c uevent.c trace.c main.c

all:
	gcc $(COPTS) $(TESTLIB_FILES)       -fPIC -shared -Wl,-soname,$(TESTLIBNAME) -o $(TESTLIBNAME)	
	gcc $(COPTS) $(LIB_FILES) $(LDOPTS) -fPIC -shared -Wl,-soname,$(LIBNAME) -o $(LIBNAME)
	gcc $(COPTS) $(LOADER_FILES) $(LDOPTS) -o $(LOADERNAME)

clean:
	rm -f *.gcda *.so fluxcapacitor a.out gmon.out