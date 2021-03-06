#
# Me2c.sco - Common product components for SCO
#
# Copyright (c) E3 Systems 1993. All Rights Reserved.
# @(#) $Name$ $Id$
#
# Executables are:
#  - natmenu (Menu Driver)
#  - oneproc (Process Tree Viewer)
#  - hisps (Accounting History Formatter)
#
# SCO - 14 character filenames
#
# NOLS = no Lachman Streams.
# If this switch is not supplied, you need -lsocket as well.
#
LIBS=-lcurses -ltermlib -lm 
CFLAGS=-DSCO -DPOSIX -DNOLS -DM_UNIX -g -I. -DAT
RANLIB = ar -ts
CC = rcc
XPGCC = rcc
#
all: hisps natmenu
	@echo 'E2 Common make finished'
#*************************************************************************
# Non-product-specific utilities
# VVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
natmenu: natmenu.c natmenu.h ansi.h comlib.a
	$(CC) $(CFLAGS) -o natmenu natmenu.c comlib.a -lcurses -ltermlib $(LIBS)
oneproc: oneproc.c
	$(CC)  $(CFLAGS) -o oneproc oneproc.c $(LIBS)
# System V.4
#	$(CC) $(CFLAGS) -o oneproc oneproc.c -lm -lelf
comp2: comp2.c
	$(CC)  $(CFLAGS) -o comp2 comp2.c $(LIBS)

hisps: hisps.o comlib.a
	$(XPGCC) $(LFLAGS) $(CFLAGS) -o hisps hisps.o comlib.a $(LIBS)
hisps.o: hisps.c
	$(XPGCC) $(CFLAGS) -c hisps.c
#
# The Library
# ===========
# The SCO C2 UNIX routines are not compatible with our malloc() (they corrupt
# memory, and our routine is very strict). 
# Therefore we use the standard routines in this case.
comlib.a: datlib.o hashlib.o natmlib.o natregex.o e2compat.o e2file.o
	ar rv $@ $?
	$(RANLIB) $@

datlib.o: datlib.c
	$(CC) $(CFLAGS) -c datlib.c

hashlib.o: hashlib.c hashlib.h
	$(CC) $(CFLAGS) -c hashlib.c

natregex.o: natregex.c natregex.h
	$(CC) $(CFLAGS) -c natregex.c

natmlib.o: natmlib.c natmenu.h natregex.h
	$(CC) $(CFLAGS) -DATT -c natmlib.c

malloc.o: malloc.c
	$(CC) $(CFLAGS) -c malloc.c

e2compat.o: e2compat.c
	$(CC) $(CFLAGS) -DSELNEED -c e2compat.c

e2file.o: e2file.c
	$(CC) $(CFLAGS) -c e2file.c
