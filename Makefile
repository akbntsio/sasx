INSTDIR = $(HOME)/bin

### YOUR OWN ENVIRONMENT ($CC $XINC $XLIB)
### FOR FreeBSD + XFree86
# XINC = -I/usr/X11R6/include
# XLIB = -L/usr/X11R6/lib
# CC = gcc
### FOR CYGWIN64 + XORG
# XINC = -I/usr/include/X11
# XLIB = -L/usr/X11R6/lib
CC = gcc

### BUILD OPTION
PROG = sasx
VER  = 2.1
MINOR = 33
DATE = 20221009
NOTES = long to int32_t, work around a sox-play error.
VERSTR = $(VER).$(MINOR)($(DATE)) $(NOTES)
DFLAGS = -g 
#DFLAGS = -O4 -m486 
CFLAGS = $(DFLAGS) -Wall $(XINC) $(DEFS) -DVERSTR='"'"$(VERSTR)"'"'

### OPTIONAL SETTING FOR THE PROGRAM
### -DWM_CLOSE     : Enable Window Manager Close Button(I'm not quite sure)
### -DANA_THREAD   : Eneble multi-threading on analyze windows
###                : require -pthread -lc_r
##### TO MAKE NON_THREAD VERSION, uncomment following lines
DEFS = -DWM_CLOSE
LIBS = -lX11 -lm 
##### TO MAKE THREAD VERSION, uncomment following lines
#DEFS = -pthread -DANA_THREAD -DWM_CLOSE
#LIBS = -lX11 -lm -lc_r

SCRIPT = 
HDR = sas.h wave.h spectro.h label.h plot.h child.h menu.h datatype.h
SRC = main.c sas.c wave.c spectro.c label.c plot.c playback.c child.c \
	analyzer.c mfcc.c menu.c mfccspect.c datatype.c
OBJ = $(SRC:.c=.o)
DOC = Readme Readme.MP Update Makefile
TGZ = sasx$(VER).$(MINOR).tgz
DIST = sasx$(VER).tgz

### DEFAULT
all:: $(PROG)

### MAINTAINANCE COMMANDS
###
help::
	@echo  "'make sasx'     builds sasx"
	@echo  "'make install'  installs $(PROG) to $(INSTDIR)"
	@echo  "'make tar'      makes archives to $(TGZ)"

install::
	make $(INSTDIR)/$(PROG)
	ls -l $(PROG) $(INSTDIR)/$(PROG)

$(INSTDIR)/$(PROG): $(PROG)
	cp -p $(PROG) $(INSTDIR)


tar:: $(TGZ)
	@ls -l $(TGZ)
$(TGZ): $(SRC) $(HDR) $(DOC)
	tar cvfz $(TGZ) $(SRC) $(HDR) $(DOC)

### BUILD COMMANDS
###

$(PROG): $(OBJ) $(HDR) 
	$(CC) -o $@ $(CFLAGS) $(XLIB) $(OBJ) $(LIBS)

clean:
	rm -f $(PROG) *.o *~ core

depend:
	makedepend -- -Y. -- $(SRC) 2>/dev/null
	echo "# make depend `date`" >> Makefile

main.o: Makefile
##########################################################################
# DO NOT DELETE THIS LINE -- make  depend  depends  on it.

main.o: sas.h label.h child.h
sas.o: sas.h wave.h spectro.h plot.h label.h menu.h datatype.h
wave.o: sas.h wave.h
spectro.o: sas.h wave.h spectro.h plot.h
label.o: sas.h label.h
plot.o: sas.h wave.h plot.h
playback.o: sas.h child.h
child.o: child.h
analyzer.o: sas.h wave.h plot.h spectro.h menu.h
menu.o: menu.h
mfccspect.o: sas.h wave.h spectro.h
datatype.o: datatype.h
# make depend Fri Aug 12 18:21:22 JST 2022
