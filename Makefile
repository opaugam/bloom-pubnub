.PHONY : all clean run

#
# - re-factored a bit the c-core SDK makefiles
# - i can't get rid of the lame "treating 'c' input as 'c++'" warning since we have to g++ the C SDK sources
#

REQUIRED    = c-core/core/pubnub_coreapi.c c-core/core/pubnub_coreapi_ex.c c-core/core/pubnub_ccore.c c-core/core/pubnub_netcore.c c-core/lib/sockets/pbpal_sockets.c c-core/lib/sockets/pbpal_resolv_and_connect_sockets.c c-core/core/pubnub_alloc_std.c c-core/core/pubnub_assert_std.c c-core/core/pubnub_generate_uuid.c c-core/core/pubnub_blocking_io.c  c-core/core/pubnub_timers.c c-core/core/pubnub_json_parse.c c-core/core/pubnub_helper.c  c-core/posix/pubnub_version_posix.c c-core/posix/pubnub_generate_uuid_posix.c c-core/posix/pbpal_posix_blocking_io.c c-core/core/pubnub_ntf_sync.c

CXX         = g++
PYTHON      = /usr/local/bin/python
INCLUDE     = -Isrc -Ipybind11/include -Ic-core/core -Ic-core/cpp -Ic-core/posix -Ic-core/freertos
CPPFLAGS    = -fPIC -g -std=c++11 -Wall -pedantic -DPUBNUB_THREADSAFE_ -DPUBNUB_LOG_LEVEL=PUBNUB_LOG_LEVEL_NONE $(INCLUDE)
LDFLAGS     = -shared -lpthread `python-config --ldflags`

DEPS        = c-core pybind11 out
TARGET      = stub.so
HEADERS     = $(shell ls src/*.hpp)
SRC         = $(shell ls src/*.cpp | xargs -n 1 basename)
OBJ         = $(patsubst %,out/%,$(SRC:.cpp=.o))

SDK         = $(shell echo $(REQUIRED) | xargs -n 1 basename)
SDK_OBJ     = $(patsubst %,out/%,$(SDK:.c=.o))

all: $(TARGET)

run: runme.py $(TARGET)
	$(PYTHON) $<

pybind11:
	git clone https://github.com/pybind/pybind11.git

c-core:
	git clone https://github.com/pubnub/c-core

$(TARGET): $(DEPS) $(SDK_OBJ) $(OBJ)
	$(CXX) -o $@  $(CPPFLAGS) $(OBJ) $(SDK_OBJ) c-core/cpp/pubnub_futres_sync.cpp $(LDFLAGS)

out/%.o: src/%.cpp $(HEADERS)
	$(CXX) -c -o $@ $< $(CPPFLAGS) `python-config --cflags`

out/%.o: c-core/core/%.c
	$(CXX) -c -o $@ $< $(CPPFLAGS)

out/%.o: c-core/posix/%.c
	$(CXX) -c -o $@ $< $(CPPFLAGS)

out/%.o: c-core/lib/sockets/%.c
	$(CXX) -c -o $@ $< $(CPPFLAGS)

clean:
	rm -rf out $(TARGET)

out:
	mkdir $@