TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

LIBRARY_IOC = asiTpx

USR_CXXFLAGS_Linux += -std=c++11

LIB_SRCS += HTTPClient.cpp
LIB_SRCS += asiTpx.cpp

DBD += asiTpxSupport.dbd

USR_SYS_LIBS_WIN32 += ws2_32

PROD_IOC = test_acquisition
test_acquisition_SRCS += test_acquisition.cpp
test_acquisition_SRCS += HTTPClient.cpp
test_acquisition_LIBS += Com

include $(ADCORE)/ADApp/commonLibraryMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

