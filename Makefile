CC = gcc
CDEBUGFLAGS = -O
#SOLARISFLAGS = -DSVR4 -DSYSV -xcg92 -xlibmil -xlibmieee
#SOL10FLAGS = "-O -DSVR4"
#LARGE_FLAGS = -O -DSVR4 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

all: bigfiles

bigfiles: bigfiles.c
	$(LINK.c) -o bigfiles bigfiles.c
	
clean:
	rm -f bigfiles bigfiles.o
