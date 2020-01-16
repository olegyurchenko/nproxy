#
# Makefile for linux.
#
CC = gcc
CXX = c++

DEBUG = -g -O3 -Wall #-pedantic#-O3
CXXFLAGS = $(DEBUG)
CFLAGS = $(DEBUG) -I. -DLINUX -DUNIX 
CPPFLAGS = -I.
LDLIBS = -lpthread 

exeext = 
dllext = .so
DEPFILE = dep.xxx


all:  nproxy$(exeext)

SRC = nproxy.c

OBJS := $(SRC:.c=.o)

nproxy$(exeext): $(OBJS)
	$(CC) -o $@ $(OBJS) $(CXXFLAGS) $(LDFLAGS) $(LDLIBS) 
.c.o:
	$(CC) -c $(CPPFLAGS) $(CXXFLAGS) $(CFLAGS) -o $@ $<
.cpp.o:
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $(CFLAGS) -o $@ $<


clean:
	-rm -f $(OBJS)

