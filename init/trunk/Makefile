RM=rm
MKDIR=mkdir
STRIP=586-strip

include VERSION

OUTDIR=build
OUTFILE=$(OUTDIR)/jem
CFG_INC=-I/opt/sombrio/jcore/staging/usr/xenomai/include \
	-I/opt/sombrio/jcore/staging/usr/include \
	-I../kmodule
CFG_LIB=-lnative -lsimpleconfig -lpthread_rt -lpthread -lcrypt 
CFG_OBJ=
COMMON_OBJ=$(OUTDIR)/main.o \
	$(OUTDIR)/libcli.o \
	$(OUTDIR)/jvmConfig.o \
	$(OUTDIR)/jvmMQ.o
OBJ=$(COMMON_OBJ) $(CFG_OBJ)

COMPILE=586-gcc -c  -pipe  -O2 -g -o "$(OUTDIR)/$(*F).o" $(CFG_INC) "$<" 
LINK=586-gcc  -pipe -O2 -g -static -o "$(OUTFILE)" $(OBJ) -L/opt/sombrio/jcore/staging/usr/xenomai/lib -L/opt/sombrio/jcore/staging/usr/lib $(CFG_LIB)

# Pattern rules
$(OUTDIR)/%.o : %.c
	$(COMPILE)

# Build rules
all: $(OUTFILE)

$(OUTFILE): $(OUTDIR)  $(OBJ)
	$(LINK)
	$(STRIP) $(OUTFILE)

$(OUTDIR):
	$(MKDIR) -p "$(OUTDIR)"

# Rebuild this project
rebuild: cleanall all

# Clean this project
clean:
	$(RM) -f $(OUTFILE)
	$(RM) -f $(OBJ)

# Clean this project and all dependencies
cleanall: clean

incrver:
	@echo "incrementing version number: $(VER)->$$(($(VER)+1))"
	@echo "DATE=`date '+\"%Y-%m-%d %H:%M\"'`" > VERSION
	@echo "USERHOST=\"$$USER @ $$HOSTNAME\"" >> VERSION
	@echo "VER=$$(($(VER)+1))" >> VERSION
	@echo "REV=0" >> VERSION
	@echo "BLD=0" >> VERSION
	rm -f version.h

incrrev:
	@echo "incrementing revision number: $(REV)->$$(($(REV)+1))"
	@echo "DATE=`date '+\"%Y-%m-%d %H:%M\"'`" > VERSION
	@echo "USERHOST=\"$$USER @ $$HOSTNAME\"" >> VERSION
	@echo "VER=$(VER)" >> VERSION
	@echo "REV=$$(($(REV)+1))" >> VERSION
	@echo "BLD=0" >> VERSION
	rm -f version.h

incrbld:
	@echo "incrementing build number: $(BLD)->$$(($(BLD)+1))"
	@echo "DATE=`date '+\"%Y-%m-%d %H:%M\"'`" > VERSION
	@echo "USERHOST=\"$$USER @ $$HOSTNAME\"" >> VERSION
	@echo "VER=$(VER)" >> VERSION
	@echo "REV=$(REV)" >> VERSION
	@echo "BLD=$$(($(BLD)+1))" >> VERSION
	rm -f version.h

version.h: VERSION
	@echo "#define BUILD_DATE \"`date '+%Y-%m-%d %H:%M'`\"" > version.h
	@echo "#define USER_HOST \"$$USER@$$HOSTNAME\"" >> version.h
	@echo "#define VERSION_NUM $(VER)" >> version.h
	@echo "#define REVISION_NUM $(REV)" >> version.h
	@echo "#define BUILD_NUM $(BLD)" >> version.h

