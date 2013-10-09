#!/bin/bash
# Set environment variables for VEGAS, bash version
echo "This script is specific to spec-hpc-xx"
echo "Setting VEGAS_DIR, CUDA, PYSLALIB, VEGAS_INCL/BIN/LIB, PATH, PYTHONPATH and LD_LIBRARY_PATH for VEGAS..."

# Note: user must set the VEGAS variable in their bash startup script

export VEGAS_DIR=$VEGAS/vegas_hpc

export CUDA=/opt/local/cuda50

export PYSLALIB=/export/home/elrohir/rcreager/rc/newt/lib/python2.7/site-packages/pyslalib

export VEGAS_INCL=/opt/local/include
#export VEGAS_BIN=/opt/local/bin
export VEGAS_LIB=/export/home/elrohir/rcreager/rc/newt/lib
export VEGAS_LIB_GCC=/usr/lib/gcc/x86_64-redhat-linux/3.4.6

export PATH=$VEGAS_DIR/bin:$CUDA/bin:$VEGAS_BIN:$PATH

export PYTHONPATH=$VEGAS/lib/python/site-packages:$VEGAS/lib/python:$VEGAS_DIR/python:/export/home/elrohir/rcreager/rc/newt/lib/python2.7/site-packages:$PYTHONPATH

export LD_LIBRARY_PATH=$PYSLALIB:$VEGAS_LIB:$CUDA/lib64:$LD_LIBRARY_PATH
