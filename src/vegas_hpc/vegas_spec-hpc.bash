#!/bin/bash
# Set environment variables for VEGAS, bash version
echo "This script is specific to spec-hpc-xx"
echo "Setting VEGAS_DIR, CUDA, PYSLALIB, VEGAS_INCL/BIN/LIB, PATH, PYTHONPATH and LD_LIBRARY_PATH for VEGAS..."

# Note: user must set the VEGAS variable in their bash startup script
LOCATION="GreenBank"

case $LOCATION in
    GreenBank)
        echo "Green Bank environment"
        export DIBAS=/home/dibas
        export CUDA=/opt/local/cuda
        ;;
    Shanghai)
        echo "Shanghai"
        export DIBAS=/opt/dibas
        export CUDA=/usr/local/cuda
        ;;
esac

export VEGAS_DIR=$VEGAS/vegas_hpc
export PYSLALIB=$DIBAS/dibaslibs/python2.6/site-packages/pyslalib
export PRESTO=$DIBAS/pulsar/src/presto
export VEGAS_INCL=$DIBAS/dibaslibs/include
#export VEGAS_BIN=/opt/local/bin
export VEGAS_LIB=$DIBAS/dibaslibs/lib
#export VEGAS_LIB_GCC=/usr/lib/gcc/x86_64-redhat-linux/3.4.6

export PATH=$VEGAS_DIR/bin:$CUDA/bin:$VEGAS_BIN:$PATH

export PYTHONPATH=$VEGAS/lib/python/site-packages:$VEGAS/lib/python:$VEGAS_DIR/python:$DIBAS/dibaslibs/lib/python2.6/site-packages:$PYTHONPATH

export LD_LIBRARY_PATH=$PYSLALIB:$VEGAS_LIB:$CUDA/lib64:$LD_LIBRARY_PATH
