# Simple makefile for the "simple" example.
CXX=g++ -Wall
CXXFLAGS=-O3
CPPFLAGS=-I..
LINK.o = $(CXX) $(CXXFLAGS) $(LDFLAGS) $(TARGET_ARCH)

all: simple

simple: simple.o
simple.o: simple.cpp cml_config.h

clean:
	rm simple simple.o
