ACLOCAL_AMFLAGS = -I config 

SUBDIRS	= doc include

pkgconfig_DATA = libmot.pc

#
# Library 
#
lib_LTLIBRARIES = src/libmot.la

src_libmot_la_SOURCES = \
    src/mot.cpp

src_libmot_la_CPPFLAGS = -Iinclude
src_libmot_la_CXXFLAGS = @LIBMOT_EXTRA_CXXFLAGS@
src_libmot_la_LIBADD =

#
# Tests
#
test_apps = \
    tests/test_header_parameters \
    tests/test_mot_headermode \
    tests/test_mot_directorymode 

tests_test_header_parameters_SOURCES = \
    tests/test_header_parameters.cpp 
tests_test_header_parameters_CPPFLAGS = -Iinclude ${CPPUNIT_CFLAGS}
tests_test_header_parameters_LDADD = -Lsrc/.libs -lmot ${CPPUNIT_LIBS}

tests_test_mot_headermode_SOURCES = \
    tests/test_mot_headermode.cpp 
tests_test_mot_headermode_CPPFLAGS = -Iinclude ${CPPUNIT_CFLAGS}
tests_test_mot_headermode_LDADD = -Lsrc/.libs -lmot ${CPPUNIT_LIBS} 

tests_test_mot_directorymode_SOURCES = \
    tests/test_mot_directorymode.cpp 
tests_test_mot_directorymode_CPPFLAGS = -Iinclude ${CPPUNIT_CFLAGS}
tests_test_mot_directorymode_LDADD = -Lsrc/.libs -lmot ${CPPUNIT_LIBS} 

check_PROGRAMS = ${test_apps}

#  Run the test cases
TESTS = $(test_apps)
XFAIL_TESTS =

EXTRA_DIST	= COPYING NEWS README.md INSTALL.md LICENCE AUTHORS ChangeLog TODO doc
