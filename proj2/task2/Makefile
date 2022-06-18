CC = gcc
CFLAGS=-g -O2 -W -Wall
LDLIBS = -lpthread

all: multiclient stockclient stockserver

multiclient: multiclient.c csapp.c
stockclient: stockclient.c csapp.c 
stockserver: stockserver.c echo.c csapp.c reqres.c treap.c sbuf.c

clean:
	rm -rf *~ multiclient stockclient stockserver *.o
