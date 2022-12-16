include /ioc/tools/driver.makefile
PROJECT = ADASItpx
BUILDCLASSES = Linux
ARCH_FILTER = ${EPICS_HOST_ARCH}
EXCLUDE_VERSIONS = 3

USR_CXXFLAGS_Linux += -std=c++11
USR_SYS_LIBS_Linux += tiffxx
USR_SYS_LIBS_WIN32 += ws2_32

SOURCES += ../asiTpxApp/src/HTTPClient.cpp
SOURCES += ../asiTpxApp/src/asiTpx.cpp

DBDS += ../asiTpxApp/src/asiTpxSupport.dbd

TEMPLATES += ../asiTpxApp/Db/asiTpx.template
TEMPLATES += ../asiTpxApp/Db/asiTpx_settings.req

QT += ../asiTpxApp/op/ui/autoconvert/asiTpx.ui
