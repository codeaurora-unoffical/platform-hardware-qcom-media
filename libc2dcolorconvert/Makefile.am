h_sources = C2DColorConverter.h
c_sources = C2DColorConverter.cpp

library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)

lib_LTLIBRARIES = libc2dcolorconvert.la
libc2dcolorconvert_la_SOURCES = $(c_sources)
libc2dcolorconvert_la_LDFLAGS = -ldl

bin_PROGRAMS = C2DDownscaleTest
C2DDownscaleTest_SOURCES = C2DDownscaleTest.cpp
C2DDownscaleTest_LDADD = libc2dcolorconvert.la -ldl -lrt
