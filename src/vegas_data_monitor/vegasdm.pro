CONFIG += debug thread

CONFIG(release, debug|release) {
    TARGET = debugvegasdm
} else {
    TARGET = vegasdm
}

unix {
    HARDWARE_PLATFORM = $$system(uname -i)
    contains( HARDWARE_PLATFORM, x86_64 ) {
        ARCH = x86_64-linux
        QWTHOME = /opt/dibas/dibaslibs/qwt-6.0.1
    }
}

DIBASHOME = /opt/dibas

TEMPLATE = app

DEPENDPATH += .

INCLUDEPATH += .

INCLUDEPATH += $$QWTHOME/include

INCLUDEPATH += $$DIBASHOME/dibaslibs/include

LIBS += $$DIBASHOME/dibaslibs/lib/libkatcp.a

LIBS += -L$$QWTHOME/lib -lqwt

QMAKE_LFLAGS += -rdynamic

# Input
HEADERS += adchist.h \
           ConfigData.h \
           ConfigFile.h \
           data_plot.h \
           data_source.h \
           EventDispatcher.h \
           hist_options.h \
           katcp_data_source.h \
           Mutex.h \
           NumericConversions.h \
           RoachInterface.h \
           simulated_data_source.h \
           strip_options.h \
           TCondition.h \
           Thread.h \
           ThreadLock.h \
           tsemfifo.h \
           VegasBankPanel.h

SOURCES += adchist.cc \
           CalendarUtils.cc \
           ConfigFile.cc \
           data_plot.cc \
           data_source.cc \
           getTimeOfDay.cc \
           hist_options.cc \
           main.cc \
           Mutex.cc \
           NumericConversions.cc \
           RoachInterface.cc \
           katcp_data_source.cc \
           simulated_data_source.cc \
           strip_options.cc \
           TimeStamp.cc \
           VegasBankPanel.cc
