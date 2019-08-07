
# NFD= $(SRCDIR)/..

CC=g++
CURRENTPATH= $(shell pwd)
# $(info $(CURRENTPATH))
CPPFLAGS += -I $(CURRENTPATH)/../include -std=c++11


all: basic_classes.o basic_methods.o
	ar rcs libNFD.a basic_methods.o basic_classes.o
