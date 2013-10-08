#!/bin/bash
# Set environment variables for VEGAS, bash version
echo "This script is specific to spec-hpc-xx"
echo "Setting VEGAS_DIR, CUDA, PYSLALIB, VEGAS_INCL/BIN/LIB, PATH, PYTHONPATH and LD_LIBRARY_PATH for VEGAS..."

# Note: user must set the VEGAS variable in their bash startup script

export DIBAS=/opt/dibas

export VEGAS_DIR=$VEGAS/vegas_hpc

export CUDA=/usr/local/cuda

export PRESTO=$DIBAS/pulsar/src/presto

#export PYSLALIB=/opt/dibas/newt/lib/python2.7/site-packages/pyslalib
export SLALIB=/opt/dibas/pulsar/src/presto/lib
export VEGAS_INCL=/opt/dibas/dibaslibs/include
#export VEGAS_BIN=/opt/local/bin
export VEGAS_LIB=/opt/dibas/dibaslibs/lib
#export VEGAS_LIB_GCC=/usr/lib/gcc/x86_64-redhat-linux/3.4.6
#export VEGAS_LIB_GCC=/usr/lib/gcc/x86_64-redhat-linux/4.4.7

export PATH=$VEGAS_DIR/bin:$CUDA/bin:$VEGAS_BIN:$PATH

export PYTHONPATH=$VEGAS/lib/python/site-packages:$VEGAS/lib/python:$VEGAS_DIR/python:/home/gbt7/newt/lib/pythong2.7/site-packages:$PYTHONPATH

export LD_LIBRARY_PATH=$SLALIB:$VEGAS_LIB:$CUDA/lib64:$LD_LIBRARY_PATH
