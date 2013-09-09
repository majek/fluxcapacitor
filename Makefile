LIBNAME=fluxcapacitor_preload.so
TESTLIBNAME=fluxcapacitor_test.so
LOADERNAME=fluxcapacitor

LDOPTS=-lrt -ldl -rdynamic
COPTS=-g -ggdb -Wall -Wextra -Wno-unused-parameter -O3 -fPIC

TESTLIB_FILES=src/testlib.c
LIB_FILES=src/preload.c
LOADER_FILES=src/wrapper.c src/parent.c src/loader.c src/uevent.c src/trace.c src/main.c

all: build test

.PHONY: build
build: $(TESTLIBNAME) $(LIBNAME) $(LOADERNAME)

$(TESTLIBNAME): Makefile $(TESTLIB_FILES)
	$(CC) $(COPTS) $(TESTLIB_FILES)	\
		-fPIC -shared -Wl,-soname,$(TESTLIBNAME) -o $(TESTLIBNAME)

$(LIBNAME): Makefile $(LIB_FILES)
	$(CC) $(COPTS) $(LIB_FILES) $(LDOPTS) \
		-fPIC -shared -Wl,-soname,$(LIBNAME) -o $(LIBNAME)

$(LOADERNAME): Makefile $(LOADER_FILES)
	$(CC) $(COPTS) $(LOADER_FILES) $(LDOPTS) \
		-o $(LOADERNAME)

FCPATH ?= $(PWD)/$(LOADERNAME)
.PHONY:test
test:
	FCPATH="$(FCPATH)" python tests/tests_basic.py

clean:
	rm -f *.gcda *.so fluxcapacitor a.out gmon.out
