CC=gcc
BIN=rgconfwatch
LIBS=-lragnar

all:
	mkdir -p ./bin
	${CC} -o ./bin/${BIN} *.c ${LIBS}

install:
	cp ./bin/${BIN} /usr/bin 
