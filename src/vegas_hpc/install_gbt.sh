#!/bin/bash
#
# Install script specific to the GBT Vegas project.
# Requirements/usage:
# * the user id must be monctrl
# * YGOR_INSTALL must be set
# 
# No VEGAS environment variables are required. They are
# computed from the location of this script, even
# if executed from outside the source tree.

# This is an unfortunate situation. For now we install the
# pre-built version from the dibas area. TODO: Make executables
# use Starlink-SLA instead of the fortran version.
export LIBSLA_DIR=/home/sandboxes/dibas/pulsar/src/presto/lib

main()
{
    do_vegas_install
}

dont_have_monctrl()
{
    idmonctrl=`id -u monctrl`
    if [ `id -u` != "$idmonctrl" ]; then
        return 0
    fi
    return 1
}

do_vegas_install()
{
    if dont_have_monctrl; then
        echo "You must be monctrl to use this script"
        exit 1
    fi
    full_script_path=`readlink -f $0`
    full_script_dir=`dirname $full_script_path`
    export VEGAS=`dirname $full_script_dir`
    if [ -z "$VEGAS" ]; then
        echo "The source directory must be specified by the VEGAS environment variable"
        echo "Hint: the directory should be the same as the location of this script"
        exit 1;
    fi
    if [ -z "$YGOR_INSTALL" ]; then
        echo "The installation directory must be specified by the YGOR_INSTALL environment variable"
        exit 1;
    fi
    cd $VEGAS/vegas_hpc;
    if [ ! -f vegas_spec-hpc.bash.nrao ]; then
        echo "the $VEGAS/vegas_spec-hpc.bash.nrao script is missing"
        exit 1
    fi
    source vegas_spec-hpc.bash.nrao
    if [ ! -f $CUDA/bin/nvcc ]; then
        echo "CUDA is set to $CUDA, but I cant find nvcc."
        echo "Cannot build on this machine. Try srbs-hpc1, kintac, or a vegas-hpc machine."
        exit 1
    fi
    cd src;
    make clean
    ./build-release
    make
    for i in check_vegas_status check_vegas_databuf clean_vegas_shmem; do
        if [ ! -f $i ]; then
            echo "$i didn't build or is missing"
            exit 1;
        else
            cp $i $YGOR_INSTALL/bin/x86_64-linux
        fi
    done
    for i in vegas_hpc_server ; do
        if [ ! -f $i ]; then
            echo "$i didn't build or is missing"
            exit 1;
        else
            cp $i $YGOR_INSTALL/exec/x86_64-linux
        fi
    done

    if [ ! -f "$LIBSLA_DIR/libsla.so" ]; then
        echo "$LIBSLA_DIR/libsla.so is missing. You may need to install this by hand."
    fi
    cp -f "$LIBSLA_DIR/libsla.so" $YGOR_INSTALL/lib/x86_64-linux
    if [ ! -f $VEGAS/../scripts/dibas/bin_scripts/re_create_data_buffers ]; then
        echo "$VEGAS/../scripts/dibas/bin_scripts/re_create_data_buffers script is missing"
        echo "Check the repo"
        exit 1
    fi
    cp $VEGAS/../scripts/dibas/bin_scripts/re_create_data_buffers $YGOR_INSTALL/bin
    echo "vegas hpc installed successfully"
}

main 
exit 0
