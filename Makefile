#
# Simple noise generator
#
# App --> PipeWire --> bluez (kernel) --> Bluetooth H/W --> remote speaker 
#
# `make`, `make all`, `make test`, and `make clean` `make config`
#


# Note: refer to `gcc -dM -E - < /dev/null` for what the compiler already defines automatically before adding things.

CWD = $(shell pwd)
OS=linux
CC=gcc
NGEN_VER:="0.1"
SRC_C_FILES:=./src/ngen.c ./src/httpd.c ./src/pwire.c
ARCH=$(shell uname -m)
BIN=./bin/ng
CFLAGS=-Wall -O2 -D_REENTRANT
INCLUDES=-I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2
LD_FLAGS=-lpipewire-0.3 -lm -lpthread

all: $(SRC_C_FILES)
	@$(CC) $(CFLAGS) $(INCLUDES) $(SRC_C_FILES) $(LD_FLAGS) -o $(BIN) 

clean:
	rm bin/*

config:
	bluetoothctl connect 08:DF:1F:00:1E:49
	bluetoothctl info 08:DF:1F:00:1E:49

test:
	test/runtests.sh

.PHONY: clean build config all test
