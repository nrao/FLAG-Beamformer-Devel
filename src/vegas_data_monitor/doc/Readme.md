# Introduction

The VEGAS Data Monitor Client is a C++, [Qt4](https://qt-project.org/)
and [Qwt](http://qwt.sourceforge.net) program that graphically displays
VEGAS BackEnd monitor points in real-time.

## Files and modules:

  * _main.cc_: the main loop of the program.  Not much going on there,
    just reads the config file and decides what the data source ought to
    be (simulated, accessor, streaming, etc.).

  * _VegasBankPanel.{h,cc}_: This file declares and implements the
    _VegasBankPanel_ class that brings all the custom widgets in this
    application together.  It provides the layout of the screen the user
    sees, connects all relevant modules together, etc.

  * _adchist.{h,cc}_: declares and defines a custom histogram class to
    display histograms of the ROACH's adcsnap values.

  * _data\_plot.{h,cc}_: declares and defines a data stripchart class to
    display the _measpwr_ data graphically.  The stripchart can be set
    up to display an arbitrary number of seconds, perform auto scaling
    or manual scaling, etc.

  * _strip\_options.{h,cc}_: creates a dialog box that provides
    stripchart options to the user.

  * _data\_source.h_: The VEGAS Data Monitor application uses a strategy
    pattern to supply the data.  This class declares a pure virtual base
    class that supplies the necessary interface, signals and slots for a
    data source.  The rest of the program will use this interface
    without concern as to how the data is being aquired.  This allows
    easy switching between a simulated data source (handy for
    development), a data source that connects to the Ygor accessor, a
    data source that uses 0MQ, etc.

  * _simulated\_data\_source.{h,cc}_: This declares and implements a
    class that derives from the base DataSource class to provide
    simulated data for the rest of the program.  Handy during
    development of other features, which can then be tested without
    needing any of the Ygor libraries or any VEGAS bank managers.

  * _device\_client\_data\_source.{h,cc}_: Provides a connection to the
    Ygor Accessor daemon to obtain data from the VEGAS Bank Manager
    samplers. So-called because it uses the Grail DeviceClient library
    which makes access to samplers and parameters relatively easy.  The
    data is pushded on to the consumer at the same rate that it is
    provided by the accessor.

  * _mustangdm.pro_: The Qt project file.  Whenever any project changes
    are made (files added, custom widgets declared with the Q_OBJECT
    macro, etc.) the files should be added here and qmake should be run
    on this file.  This will then generate the MOC files and Makefile.
