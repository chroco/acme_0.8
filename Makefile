
#	Chad Coates
#	ECE 373
# Homework #6
# May 16, 2017
#
# supercool makefile,
# almost...
#

HW=6
HOMEWORK=hw$(HW).sh

KERNEL_DIR = /lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)
KO=acme.ko
obj-m += acme.o 

CC=gcc
USER=ninja #name of the user space binary
USER_CFLAGS= -O0 -g -Wall -pthread 
FLAGS= -lpci -lz
DEPS=
SRCS=usr_acme.c
OBJS = $(SRCS:.c=.o)

default: $(KO) 
	@echo $(KO) compiled

all: $(KO) $(USER)
	@echo $(KO) and $(USER) compiled
 
ko: $(KO)
	@echo $(KO) compiled

$(KO): 
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules

user: $(USER)
	@echo $(USER) compiled

$(USER): $(OBJS) $(DEPS)
	$(CC) $(USER_CFLAGS) -o $(USER) $(OBJS) $(FLAGS)

.c.o: 
	$(CC) $(USER_CFLAGS) -c $^  -o $@

clean: 
	rm -f *.o

cleanall: 
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) clean
	rm -f *.o $(USER)

install:
	sudo insmod ./$(KO)

remove:
	sudo rmmod $(KO)

hw:
	sudo ./$(HOMEWORK)
	
