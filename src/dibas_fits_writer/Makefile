OPT_FLAGS=

# Local vars setup by some bash script somewhere
CUDA=/opt/local/cuda
DIBAS=/home/dibas
PYSLALIB=/home/dibas/pulsar/src/presto/lib
VEGAS_LIB=/home/dibas/dibaslibs/lib
PRESTO=/home/dibas/pulsar/src/presto

USER_INCLUDES += -I$(CUDA)/include
#USER_INCLUDES += -I/opt/local/include
USER_INCLUDES += -I$(PYSLALIB)
# Relative path include to vegas_hpc/src
HPC_PATH=../vegas_hpc/src/
USER_INCLUDES += -I$(HPC_PATH)
USER_INCLUDES += -I$(DIBAS)/dibaslibs/include

SHAREDLIBRARYTARGET=libVegasFitsIO.so
LIBRARYTARGET=libVegasFitsIO.a
C__COMPILE=gcc
CXXCOMPILE=g++ -pthread
AR_CMD=ar
CUDA_COMPILE=nvcc


C__FLAGS=-O3 -g -Wall -D_GNU_SOURCE -I. $(USER_INCLUDES) $(OPT_FLAGS)
CXXFLAGS=-O3 -g -pthread  -Wall -I. $(USER_INCLUDES) -msse2 -lrt
CUDA_FLAGS= -I. $(USER_INCLUDES) -arch=sm_35 -I$(CUDA)/include

EXECUTABLE=vegasFitsWriter

all: $(EXECUTABLE)

EXTRA_SOURCES =vegas_status.c
EXTRA_SOURCES+=vegas_databuf.c
EXTRA_SOURCES+=vegas_error.c
EXTRA_SOURCES+=vegas_params.c
EXTRA_SOURCES+=vegas_thread_args.c
EXTRA_SOURCES+=vegas_time.c
EXTRA_SOURCES+=hashpipe_ipckey.c
EXTRA_SOURCES+=hput.c
EXTRA_SOURCES+=hget.c
EXTRA_SOURCES+=sla.c
# Extra objects
EXTRA_OBJECTS =vegas_status.o
EXTRA_OBJECTS+=vegas_databuf.o
EXTRA_OBJECTS+=vegas_error.o
EXTRA_OBJECTS+=vegas_params.o
EXTRA_OBJECTS+=vegas_thread_args.o
EXTRA_OBJECTS+=vegas_time.o
EXTRA_OBJECTS+=hashpipe_ipckey.o
EXTRA_OBJECTS+=hput.o
EXTRA_OBJECTS+=hget.o
EXTRA_OBJECTS+=sla.o
EXTRA_OBJECTS+=privilege_management.o
#
vpath %.c $(HPC_PATH)
# Generate the C Source file list from the files in the current directory.
C__SOURCES  += $(wildcard *.c )
C__SOURCES  += $(EXTRA_SOURCES)
CXXSOURCES  += $(wildcard *.cc )
CUDA_SOURCES += $(wildcard *.cu)
C__OBJECTS = ${C__SOURCES:.c=.o}
CXXOBJECTS = ${CXXSOURCES:.cc=.o}
CUDA_OBJECTS= ${CUDA_SOURCES:.cu=.o}

LIBS = -L$(VEGAS_LIB) -L$(PRESTO)/lib -lcfitsio -lsla -lcap

# C compilation rules
.c.o:
	@echo Compiling $<
	$(C__COMPILE) $(C__FLAGS) -c $<
#
# C++ compilation rules
.cc.o:
	@echo Compiling $<
	$(CXXCOMPILE) $(CXXFLAGS) -c $<
#
# CUDA compilation rule
#.cu.o:
%.o: %.cu
	@echo Compiling $<
	$(CUDA_COMPILE) -c $(CUDA_FLAGS) $<


${EXECUTABLE}: ${CXXOBJECTS} ${C__OBJECTS} ${CUDA_OBJECTS} ${EXTRA_OBJECTS}
	@echo "Building the $(@F) executable"
	$(CXXCOMPILE) $(CXXFLAGS) -o $@ ${CXXOBJECTS} ${CUDA_OBJECTS} ${EXTRA_OBJECTS} ${LIBS}

${SHAREDLIBRARYTARGET}: ${C__OBJECTS} ${CUDA_OBJECTS}
	@echo "Building the $(@F) library."
	$(C__COMPILE) $(C__FLAGS) -shared -o $@ ${C__OBJECTS} ${CUDA_OBJECTS}

${LIBRARYTARGET}: ${C__OBJECTS} ${CUDA_OBJECTS} ${CXXOBJECTS}
	@echo "Building the $(@F) library."
	$(AR_CMD) ruv $@ ${C__OBJECTS} ${CUDA_OBJECTS} ${CXXOBJECTS}

clean:
	rm -f *.o $(EXECUTABLE) $(LIBRARYTARGET) $(SHAREDLIBRARYTARGET)

help:
	@echo "EXECUTABLE is  $(EXECUTABLE)"
	@echo "C__SOURCES are $(C__SOURCES)"
	@echo "C__OBJECTS are $(C__OBJECTS)"
	@echo "CXXSOURCES are $(CXXSOURCES)"
	@echo "CXXOBJECTS are $(CXXOBJECTS)"
	@echo "CUDA_SOURCES are $(CUDA_SOURCES)"
	@echo "CUDA_OBJECTS are $(CUDA_OBJECTS)"
	@echo "EXTRA_OBJECTS are $(EXTRA_OBJECTS)"
	@echo "EXTRA_SOURCES are $(EXTRA_SOURCES)"

doc:
	@doxygen doxfile.conf
