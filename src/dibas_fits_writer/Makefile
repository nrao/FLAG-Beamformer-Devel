
OPT_FLAGS=

CUDADIR=/opt/local/cuda42

USER_INCLUDES += -I$(CUDADIR)include
USER_INCLUDES += -I/opt/local/include
USER_INCLUDES += -I$(PYSLALIB)
# Relative path include to vegas_hpc/src
HPC_PATH=../vegas_hpc/src/
USER_INCLUDES += -I$(HPC_PATH)

SHAREDLIBRARYTARGET=libVegasFitsIO.so
LIBRARYTARGET=libVegasFitsIO.a
C__COMPILE=gcc 
CXXCOMPILE=g++ -pthread
AR_CMD=ar
CUDA_COMPILE=nvcc


C__FLAGS=-g -Wall -D_GNU_SOURCE -I. $(USER_INCLUDES) $(OPT_FLAGS)  
CXXFLAGS=-g -pthread  -Wall -I. $(USER_INCLUDES) -I/usr/include/cfitsio
CUDA_FLAGS= -I. $(USER_INCLUDES) -arch=sm_20 -I$(CUDA)/include 

#LIBRARYTARGET=lib$(THISDIR).a
#SHAREDLIBRARYTARGET=lib$(THISDIR).so
EXECUTABLE=vegasFitsWriter

all: $(EXECUTABLE)

#EXTRA_SOURCES =$(HPC_PATH)/vegas_status.c
#EXTRA_SOURCES+=$(HPC_PATH)/vegas_databuf.c
#EXTRA_SOURCES+=$(HPC_PATH)/vegas_error.c
#EXTRA_SOURCES+=$(HPC_PATH)/vegas_params.c
#EXTRA_SOURCES+=$(HPC_PATH)/vegas_thread_args.c
#EXTRA_SOURCES+=$(HPC_PATH)/vegas_time.c
#EXTRA_SOURCES+=$(HPC_PATH)/hput.c
#EXTRA_SOURCES+=$(HPC_PATH)/hget.c
#EXTRA_SOURCES+=$(HPC_PATH)/sla.c
# Extra objects
EXTRA_OBJECTS =vegas_status.o
EXTRA_OBJECTS+=vegas_databuf.o
EXTRA_OBJECTS+=vegas_error.o
EXTRA_OBJECTS+=vegas_params.o
EXTRA_OBJECTS+=vegas_thread_args.o
EXTRA_OBJECTS+=vegas_time.o
EXTRA_OBJECTS+=hput.o
EXTRA_OBJECTS+=hget.o
EXTRA_OBJECTS+=sla.o
#
VPATH=$(HPC_PATH)
# Generate the C Source file list from the files in the current directory.
C__SOURCES  += $(wildcard *.c )
CXXSOURCES  += $(wildcard *.cc )
CUDA_SOURCES += $(wildcard *.cu)
C__OBJECTS = ${C__SOURCES:.c=.o} 
CXXOBJECTS = ${CXXSOURCES:.cc=.o}
CUDA_OBJECTS= ${CUDA_SOURCES:.cu=.o}

LIBS = -L$(VEGAS_LIB) -lcfitsio -lsla 

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