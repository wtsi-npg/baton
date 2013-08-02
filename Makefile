
CC = gcc
CFLAGS = -Wall -fPIC -std=c99 -g
LIBPATH = -L./ -L$(IRODS_HOME)/lib/core/obj
LDFLAGS = $(LIBPATH) -lRodsAPIs -lzlog -lpthread

IRODS_HOME=/usr/local/lib/irods

INCLUDES=-I$(IRODS_HOME)/lib/api/include -I$(IRODS_HOME)/server/core/include -I$(IRODS_HOME)/server/drivers/include -I$(IRODS_HOME)/server/icat/include -I$(IRODS_HOME)/server/re/include -I$(IRODS_HOME)/lib/core/include -I$(IRODS_HOME)/lib/md5/include -I$(IRODS_HOME)/lib/sha1/include


.PHONY: test clean install

all: libbaton.so bmeta

bmeta: bmeta.o libbaton.so
	$(CC) $< $(LDFLAGS) -lbaton -o $@

libbaton.so: baton.o
	$(CC) -shared $(LDFLAGS) -o $@ $<

test : test_baton.o

%.o : %.c
	$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $<

clean:
	rm -f bmeta
	rm -f *.so
	rm -f *.o
