#!/bin/bash
#
# Install script specific to the DIBAS project.
# Requirements/usage:
# ./install_dibas $DIBAS_DIR
# Where:
#    DIBAS_DIR is the installation root (directory must exist)
#    python executable must be in current PATH
# Error checking
#

main()
{
    check_pwd
    create_directories $1 $2 $3 $4
    install_vegas_python_files $1 $2 $3 $4
    install_vegas_daq $1 $2 $3 $4
    install_guppi_python_files $1 $2 $3 $4
    install_guppi_daq $1 $2 $3 $4
    install_dibas_python $1 $2 $3 $4
    install_katcp_lib $1 $2 $3 $4
    create_dibas_bash $1 $2 $3 $4
}

have_root()
{
    if [ `id -u` != "0" ]; then
        return 1
    fi
    return 0
}

check_pwd()
{
    set -x
    savedir=`pwd`
    curdir=`pwd`
    A=`basename $curdir`
    cd ..
    curdir=`pwd`
    B=`basename $curdir`
    cd ..
    if [ $A != "dibas" -o $B != "scripts" ]; then
        echo "This script must be run from the vegas_devel/scripts/dibas directory"
        echo "Aborting installation"
        cd $savedir
        exit 1
    fi
    VEGDIR=`pwd`
    if [ ! -z "$VEGAS_SOURCE" -a -x "$VEGAS_SOURCE" ]; then
        cd $VEGAS_SOURCE/scripts/dibas
        echo
        echo "Note: Using vegas source tree in $VEGAS_SOURCE"
        echo "(Specified by the VEGAS_SOURCE environment variable)"
        echo "Unset this to use the source tree in the current directory"
        echo -n "Continue y/n [n]"
        read response
        if [ $response != "y" ]; then
            echo "Aborting installation"
            cd $savedir
            exit 1
        fi
    fi
    if [ ! -z $VEGAS_SOURCE ]; then
        export VEGDIR=$VEGAS_SOURCE
    fi

    # At this point we should be in the root of the vegas_dev directory
    # Check to see if gupp_daq is present
    pwd
    ls -l ..
    export GUPDIR=`pwd`/../guppi_daq
    if [ ! -x  ../guppi_daq -a -z "$GUPPI_SOURCE" ]; then
        echo "This installation script requires the two directories"
        echo "vegas_dev and guppi_daq to be in the same parent directory"
        echo "If the guppi source tree is in another location, first set"
        echo "GUPPI_SOURCE and re-run the script"
        echo "Aborting installation"
        cd $savedir
        exit 1
    fi
    if [ ! -z $GUPPI_SOURCE ]; then
        export GUPDIR=$GUPPI_SOURCE
    fi
    set +x
}

create_directories()
{

    if [ ! -x $1/versions ]; then
        echo $1/versions directory does not exist.
        echo "Creating it"
        install -o $2 -g $3 -d $1/versions
        install -o $2 -g $3 -d $1/etc/config
        install -o $2 -g $3 -d $1/versions/$4
    fi
    for i in exec/x86_64-linux bin/x86_64-linux lib/x86_64-linux lib/python;
    do
        install -o $2 -g $3 -d $1/versions/$4/$i
    done
    for i in exec bin lib;
    do
        rm -f $1/$i
        ln -s $1/versions/$4/$i $1/$i
    done
}

install_vegas_python_files()
{
    savedir=`pwd`
    cd src/vegas_hpc
    python setup.py install --install-lib=$1/lib/python
    if have_root; then
        chown -R $2:$3 $1/lib/python
    fi
    cd $savedir
}

install_dibas_python()
{
    savedir=`pwd`
    cd scripts/dibas
    for i in *.py; do
        install -o $2 -g $3 -m 664 $i $1/lib/python
    done
    install -m 664 -o $2 -g $3 dibas.conf $1/etc/config
    cd $savedir
    cd scripts/dibas/bin_scripts
    for i in dealer dibas_status  init_status_memory  player  re_create_data_buffers; do
        install -m 775 -o $2 -g $3 $i $1/bin
    done
    install -m 755 -o $2 -g $3 setVersion $1
    cd $savedir
}

install_vegas_daq()
{

    savedir=`pwd`
    cd src
    for i in  \
        vegas_hpc/src/vegas_hpc_server \
        dibas_fits_writer/vegasFitsWriter ;
    do
        if [ -x  $i ]; then
            j=`basename $i`
            if have_root; then
                install -o root -g $3 -m 6755 $i $1/exec/x86_64-linux
                echo "$j installed suid root"
            else
                install -o $2 -g $3 -m 6755 $i $1/exec/x86_64-linux
                echo "$j installed suid $2"
            fi
        else
            echo "$i doesn't exist"
            g=`dirname $i`
            echo "cd to $g and run make first"
        fi
    done
    for i in  \
        vegas_hpc/src/check_vegas_status \
        vegas_hpc/src/check_vegas_databuf \
        vegas_hpc/src/clean_vegas_shmem ;
    do
        if [ -x  $i ]; then
            j=`basename $i`
            if have_root; then
                install -o root -g $3 -m 6755 $i $1/bin/x86_64-linux
                echo "$j installed suid root"
            else
                install -o $2 -g $3 -m 6755 $i $1/bin/x86_64-linux
                echo "$j installed suid $2"
            fi
        else
            echo "$i doesn't exist"
            g=`dirname $i`
            echo "cd to $g and run make first"
        fi
    done


    cd $savedir
    cd src/vegas_hpc
    for i in *.dat; do
        install -m 664 -o $2 -g $3 $i $1/etc/config
    done
    cd $savedir
}

install_guppi_daq()
{
    savedir=`pwd`
    cd $GUPDIR
    if [ -x src/guppi_daq_server ]; then
        if have_root; then
            install -o root -g $3 -m 6755 src/guppi_daq_server $1/exec/x86_64-linux
            echo "guppi_daq_server installed suid root"
        else
            install -o $2 -g $3 -m 6755 src/guppi_daq_server $1/exec/x86_64-linux
            echo "guppi_daq_server installed suid $2"
        fi
    else
        echo "guppi_daq_server doesn't exist"
        echo "run make first"
    fi

    for i in  \
        src/check_guppi_status \
        src/check_guppi_databuf \
        src/clean_guppi_shmem ;
    do
        if [ -x  $i ]; then
            j=`basename $i`
            if have_root; then
                install -o root -g $3 -m 6755 $i $1/bin/x86_64-linux
                echo "$j installed suid root"
            else
                install -o $2 -g $3 -m 6755 $i $1/bin/x86_64-linux
                echo "$j installed suid $2"
            fi
        else
            echo "$i doesn't exist"
            g=`dirname $i`
            echo "cd to $g and run make first"
        fi
    done

    cd $savedir
}
install_guppi_python_files()
{
    savedir=`pwd`
    cd $GUPDIR
    python setup.py install --install-lib=$1/lib/python
    if have_root; then
        chown -R $2:$3 $1/lib/python
    fi
    cd $savedir
}

install_katcp_lib()
{
    echo "cd over to the katcp subdirectory in the katcp repository "
    echo "type:"
    echo " make PREFIX=$1/lib/python install"
    echo "Then install the katcp wrappers"
}

create_dibas_bash()
{
    if [ ! -f $1/dibas.bash ]; then
       echo
       echo "A dibas.bash setup script is required."
       echo "This step must be done manually"
    fi
}

# Now start the process:
if have_root; then
    echo "You must be root to properly install guppi"
    echo "Installation may fail to set directory and executable permissions"
    echo "Attempting to continue anyway ..."
    echo
fi

if [ ! "$#" -gt 2 ]; then
    echo "The installation directory, user and group must be specified"
    echo "Releasename is optional, if not specified release will be used"
    echo "Usage:"
    echo "$0 /some/installdir dibasuser dibasgroup [ releasename ]"
    echo "Installation aborted"
    exit 1
else
    export INSTDIR=$1
    export DIBASUSR=$2
    export DIBASGRP=$3
fi
if [ -z "$4" ]; then
    echo "Defaulting releasename to release"
    export VERSION=release
else
    export VERSION=$4
fi

main $INSTDIR $DIBASUSR $DIBASGRP $VERSION
exit 0
