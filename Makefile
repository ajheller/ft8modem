#
#
#   Makefile
#
#   Copyright (C) 2023-2025 by Matt Roberts.
#   License: GNU GPL3 (www.gnu.org)
#
#

# various build target groups
TARGETS1=ft8modem
TARGETS2=ft8encode
TARGETS3=test_abstime test_encode cpucores
TARGETS4=test_decode decimate
TARGETS5=test_cosine test_es
TARGETS6=af2udp
TARGETS7=ft8tail

# these are the Python scripts to install
SCRIPTS=ft8cat ft8collect ft8encode ft8modem ft8qso ft8report ft8sdr ft8swl ft8range onlycalls

# these are the python support modules to install
SCRIPT_MODULES=\
	alltxt.py bands.py callsigns.py computefb.py \
	conflicts.py fdutils.py fragment.py messages.py \
	parsing.py pretty.py rigctld.py spots.py prefixes.py \
	prefixes.txt tailall.py tailfile.py udpaf.py version.py \
	gridsquares.py

# installation targets
BINDIR=/usr/local/bin
LIBDIR=/usr/local/lib/ft8modem

# dependencies
TARGETS=$(TARGETS1) $(TARGETS2) $(TARGETS3) $(TARGETS4) $(TARGETS5) $(TARGETS6) $(TARGETS7)
OBJECTS1=encode.o nlimits.o pipes.o snddev.o
OBJECTS2=encode.o nlimits.o
OBJECTS3=encode.o
OBJECTS4=pipes.o
OBJECTS5=nlimits.o
OBJECTS6=
OBJECTS7=
LIBS1=-lm -lrtaudio -lsndfile -lpthread
LIBS2=-lm -lsndfile
LIBS3=-lm
LIBS4=-lm -lsndfile -lpthread
LIBS5=-lm -lsndfile
LIBS6=-lm
LIBS7=

# build flags for C++ debugging
CDEBUG=-Wall -ggdb -D_DEBUG

# rule patterns
.cpp.o:
	g++ -Wall $(CFLAGS) $(CDEBUG) -c $<
.cc.o:
	g++ -Wall $(CFLAGS) $(CDEBUG) -c $<
.c.o:
	gcc -Wall $(CFLAGS) $(CDEBUG) -c $<

# default target
all: $(TARGETS)

# target group rules
$(TARGETS1): %: %.o $(OBJECTS1)
	g++ $(CFLAGS) $(CDEBUG) -o $@ $@.o $(OBJECTS1) $(LIBS1)

$(TARGETS2): %: %.o $(OBJECTS2)
	g++ $(CFLAGS) $(CDEBUG) -o $@ $@.o $(OBJECTS2) $(LIBS2)

$(TARGETS3): %: %.o $(OBJECTS3)
	g++ $(CFLAGS) $(CDEBUG) -o $@ $@.o $(OBJECTS3) $(LIBS3)

$(TARGETS4): %: %.o $(OBJECTS4)
	g++ $(CFLAGS) $(CDEBUG) -o $@ $@.o $(OBJECTS4) $(LIBS4)

$(TARGETS5): %: %.o $(OBJECTS5)
	g++ $(CFLAGS) $(CDEBUG) -o $@ $@.o $(OBJECTS5) $(LIBS5)

$(TARGETS6): %: %.o $(OBJECTS6)
	g++ $(CFLAGS) $(CDEBUG) -o $@ $@.o $(OBJECTS6) $(LIBS6)

$(TARGETS7): %: %.o $(OBJECTS7)
	gcc $(CFLAGS) $(CDEBUG) -o $@ $@.o $(OBJECTS7) $(LIBS7)

# meta-targets
clean:
	rm -f $(TARGETS) core *.o *.wav
	rm -f Makefile.bak decoded.txt jt9_wisdom.dat timer.out avemsg.txt gmon.out
	rm -f ALL.TXT ALL_WSPR.TXT hashtable.txt wspr_spots.txt wspr_timer.out wspr_wisdom.dat
	rm -rf __pycache__

strip: $(TARGETS)
	strip --strip-unneeded $(TARGETS)

rebuild:
	make -j1 clean
	make

install: strip
	# install the compiled executables in $(BINDIR)
	for i in $(TARGETS1) $(TARGETS2) $(TARGETS6) $(TARGETS7) ; do install -o root -g root -m 0755 $$i $(BINDIR) ; done
	# create the folder for the scripts and modules
	install -o root -g root -m 0755 -d $(LIBDIR)
	# install the scripts in $(LIBDIR)
	for i in $(SCRIPTS) ; do install -o root -g root -m 0755 $$i $(LIBDIR) ; done
	# link the top-level scripts into $(BINDIR)
	for i in $(SCRIPTS) ; do test -e $(BINDIR)/$$i || ln -sv $(LIBDIR)/$$i $(BINDIR)/ ; done
	# install modules in $(LIBDIR)
	for i in $(SCRIPT_MODULES) ; do install -o root -g root -m 0644 $$i $(LIBDIR) ; done

uninstall:
	for i in $(SCRIPTS) ; do rm -vf $(BINDIR)/$$i ; done
	for i in $(TARGETS1) $(TARGETS2) $(TARGETS6) ; do rm -vf $(BINDIR)/$$i ; done
	for i in $(SCRIPTS) $(SCRIPT_MODULES) ; do rm -vf $(LIBDIR)/$$i ; done

dep depend:
	makedepend -Y *.c *.cc *.cpp 2>/dev/null

ignore:
	svn propset svn:ignore -F .svnignore .

# EOF - everything below this line is from 'makedepend'
# DO NOT DELETE

af2udp.o: version.h
cpucores.o: cores.h stype.h
decimate.o: decimate.h sf.h stype.h
encode.o: stype.h jt65.h encode.h
ft8encode.o: encode.h mfsk.h shape.h nlimits.h IFilter.h osc.h es.h sf.h
ft8encode.o: stype.h presets.h
ft8modem.o: snddev.h sc.h mfsk.h shape.h nlimits.h IFilter.h osc.h es.h
ft8modem.o: decode.h sf.h stype.h clock.h cores.h locker.h pathtools.h
ft8modem.o: pipes.h encode.h FirFilter.h WindowFunctions.h FilterTypes.h
ft8modem.o: FilterUtils.h decimate.h presets.h level.h version.h
pipes.o: stype.h locker.h pipes.h
snddev.o: snddev.h sc.h mfsk.h shape.h nlimits.h IFilter.h osc.h es.h
snddev.o: decode.h sf.h stype.h clock.h cores.h locker.h pathtools.h pipes.h
snddev.o: encode.h FirFilter.h WindowFunctions.h FilterTypes.h FilterUtils.h
snddev.o: decimate.h presets.h level.h
test_abstime.o: clock.h
test_cosine.o: shape.h nlimits.h IFilter.h
test_decode.o: decode.h sf.h stype.h clock.h cores.h locker.h pathtools.h
test_decode.o: pipes.h
test_encode.o: encode.h
test_es.o: es.h nlimits.h IFilter.h
nlimits.o: nlimits.h
